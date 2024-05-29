#include "CustomAllocator.h"

#include <iostream>
#include <algorithm>
#include <cstddef>

using namespace experimental;


bool& experimental::debug()
{
    static bool s_debug = false;
    return s_debug;
}

CustomMemorySlots::CustomMemorySlots(size_t size)
{
    offsets.push_back(0);
    offsets.push_back(size);

    availableSlots.push_back(0);

    if (debug())
    {
        std::cout<<"CustomMemorySlots::CustomMemorySlots(("<<size<<") "<<std::endl;
        for(auto& offset : offsets)
        {
            std::cout<<"    "<<offset<<std::endl;
        }
    }
}

CustomMemorySlots::OptionalOffset CustomMemorySlots::reserve(size_t size, size_t alignment)
{
    if (availableSlots.empty())
    {
        if (debug()) std::cout<<"CustomMemorySlots::reserve("<<size<<") empty(), no space found "<<std::endl;
        return {false, 0};
    }

    for(auto itr = availableSlots.rbegin(); itr != availableSlots.rend(); ++itr)
    {
        size_t slotIndex = *itr;
        size_t slotSize = offsets[slotIndex+1] - offsets[slotIndex];
        if (size == slotSize)
        {
            availableSlots.erase(itr.base());
            if (debug()) std::cout<<"CustomMemorySlots::reserve("<<size<<") complete reserve, slotIndex = "<<slotIndex<<std::endl;
            return {true, offsets[slotIndex]};
        }
        else if (size < slotSize)
        {
            offsets.insert(offsets.begin() + slotIndex + 1, offsets[slotIndex] + size);
            (*itr) = slotIndex+1;

            if (debug()) std::cout<<"CustomMemorySlots::reserve("<<size<<") Partial reserve, slotIndex = "<<slotIndex<<std::endl;

            return {true, offsets[slotIndex]};
        }
    }

    if (debug()) std::cout<<"CustomMemorySlots::reserve("<<size<<") no space found "<<std::endl;

    return {false, 0};
}

bool CustomMemorySlots::release(size_t offset, size_t size)
{
    auto offset_itr = std::lower_bound(offsets.begin(), offsets.end(), offset);
    if (offset_itr != offsets.end())
    {
        size_t index = offset_itr - offsets.begin();
        auto available_itr = std::upper_bound(availableSlots.begin(), availableSlots.end(), index);
        if (available_itr != availableSlots.end())
        {
            if (debug()) std::cout<<"CustomMemorySlots::release("<<offset<<", "<<size<<") *offset_itr = "<<*offset_itr<<", found slot, index = "<<index<<" inserting at "<<*available_itr<<std::endl;
            availableSlots.insert(available_itr, index);
        }
        else
        {
            if (debug()) std::cout<<"CustomMemorySlots::release("<<offset<<", "<<size<<") *offset_itr = "<<*offset_itr<<", found slot, index = "<<index<<" pushing to end "<<std::endl;
            availableSlots.push_back(index);
        }
        return true;
    }
    if (debug()) std::cout<<"CustomMemorySlots::release("<<offset<<", "<<size<<") not found"<<std::endl;
    return false;
}

CustomMemoryBlock::CustomMemoryBlock(size_t in_blockSize, size_t in_alignment) :
    memorySlots(in_blockSize),
    alignment(in_alignment),
    blockSize(in_blockSize)
{
    if (debug()) std::cout<<"CustomMemoryBlock::CustomMemoryBlock("<<in_blockSize<<", "<<in_alignment<<")"<<std::endl;

    block_alignment = std::max(alignment, alignof(std::max_align_t));
    block_alignment = std::max(block_alignment, size_t{16});

    memory = static_cast<uint8_t*>(operator new (blockSize, std::align_val_t{block_alignment}));
}

CustomMemoryBlock::~CustomMemoryBlock()
{
    operator delete (memory, std::align_val_t{block_alignment});
}

void* CustomMemoryBlock::allocate(std::size_t size)
{
    auto result = memorySlots.reserve(size, alignment);
    if (result.first)
    {
        return static_cast<void*>(memory + result.second);
    }
    return nullptr;
}

bool CustomMemoryBlock::deallocate(void* ptr, std::size_t size)
{
    if (ptr >= memory && ptr < memory+blockSize)
    {
        size_t offset = static_cast<uint8_t*>(ptr) - memory;
        if (memorySlots.release(offset, size))
        {
            return true;
        }
        else
        {
            if (debug()) std::cout<<"Could not find slot to release"<<std::endl;
            return false;
        }
    }

    return true;
}

void CustomMemoryBlock::report(std::ostream& out) const
{
    out << "MemoryBlock "<<this<<std::endl;
    out << "   alignment = "<<alignment<<std::endl;
    out << "   block_alignment = "<<block_alignment<<std::endl;
    out << "   blockSize = "<<blockSize<<", memory = "<<static_cast<void*>(memory)<<std::endl;

    out << "   memorySlots.offsets = "<< memorySlots.offsets.size()<< " {"<<std::endl;
    for(auto& offset : memorySlots.offsets) out <<" "<<offset;
    out << "}"<< std::endl;

    out << "   memorySlots.availableSlots = "<< memorySlots.availableSlots.size()<< " {"<<std::endl;
    for(auto& available : memorySlots.availableSlots) out <<" "<<available;
    out << "}"<<std::endl;
}

CustomAllocator::CustomAllocator(std::unique_ptr<Allocator> in_nestedAllocator) :
    vsg::Allocator(std::move(in_nestedAllocator))
{
    size_t alignment = 4;
    size_t new_blockSize = size_t(20) * size_t(1024) * size_t(1024);

    auto block = std::make_shared<CustomMemoryBlock>(new_blockSize, alignment);
    memoryBlock = std::move(block);
    if (debug()) std::cout << "CustomAllocator()" << this << " "<<block<<std::endl;
}

CustomAllocator::~CustomAllocator()
{
    if (debug()) std::cout << "~CustomAllocator() " << this << std::endl;
}

void CustomAllocator::report(std::ostream& out) const
{
    out << "CustomAllocator::report() " << allocatorMemoryBlocks.size() << std::endl;

    if (memoryBlock)
    {
        memoryBlock->report(out);
    }

}

void* CustomAllocator::allocate(std::size_t size, vsg::AllocatorAffinity /*allocatorAffinity*/)
{
    std::scoped_lock<std::mutex> lock(mutex);

    if (void* data = memoryBlock->allocate(size); data != nullptr) return data;

    return operator new (size); //, std::align_val_t{default_alignment});
}

bool CustomAllocator::deallocate(void* ptr, std::size_t size)
{
    std::scoped_lock<std::mutex> lock(mutex);

    if (memoryBlock->deallocate(ptr, size)) return true;

    if (nestedAllocator && nestedAllocator->deallocate(ptr, size)) return true;

    operator delete (ptr);
    return true;
}

