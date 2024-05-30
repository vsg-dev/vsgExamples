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
#if 1
    size_t num_slots = size / 32;
    offsets.reserve(num_slots);
    availableSlots.reserve(num_slots);
#endif

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
        Index slotIndex = *itr;
        Offset slotSize = offsets[slotIndex+1] - offsets[slotIndex];
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
        Index index = offset_itr - offsets.begin();
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

CustomMemoryBlock::CustomMemoryBlock(const std::string& in_name, size_t in_blockSize, size_t in_alignment) :
    name(in_name),
    memorySlots(in_blockSize),
    alignment(in_alignment),
    blockSize(in_blockSize)
{
    if (debug()) std::cout<<"CustomMemoryBlock::CustomMemoryBlock("<<in_blockSize<<", "<<in_alignment<<")"<<std::endl;

    block_alignment = std::max(alignment, alignof(std::max_align_t));
    block_alignment = std::max(block_alignment, size_t{16});

    //memory = static_cast<uint8_t*>(operator new (blockSize, std::align_val_t{block_alignment}));
    memory = static_cast<uint8_t*>(operator new (blockSize));
    memory_end = memory + blockSize;
}

CustomMemoryBlock::~CustomMemoryBlock()
{
    //operator delete (memory, std::align_val_t{block_alignment});
    operator delete (memory);
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
    if (within(ptr))
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
    out << "MemoryBlock "<<this<<" "<<name<<std::endl;
    out << "    alignment = "<<alignment<<std::endl;
    out << "    block_alignment = "<<block_alignment<<std::endl;
    out << "    blockSize = "<<blockSize<<", memory = "<<static_cast<void*>(memory)<<std::endl;

    out << "    memorySlots.offsets = "<< memorySlots.offsets.size()<< ", capacity = "<<memorySlots.offsets.capacity()<<" {"<<std::endl;
#if 0
    for(auto& offset : memorySlots.offsets) out <<" "<<offset;
    out << "}"<< std::endl;
#endif
    out << "    memorySlots.availableSlots = "<< memorySlots.availableSlots.size()<< ", capacity = "<<memorySlots.availableSlots.capacity()<< " {"<<std::endl;
#if 0
    for(auto& available : memorySlots.availableSlots) out <<" "<<available;
    out << "}"<<std::endl;
#endif

    size_t totalSizeAllocated = 0;
    size_t numAllocated = 0;
    for(size_t i = 0; i<memorySlots.offsets.size()-2; ++i)
    {
        totalSizeAllocated += memorySlots.offsets[i+1] - memorySlots.offsets[i];
        ++numAllocated;
    }

    if (numAllocated>0)
    {
        double averageSize = double(totalSizeAllocated) / double(numAllocated);
        out<<"     averageSize = "<<averageSize<<std::endl;
    }
}

CustomAllocator::CustomAllocator(std::unique_ptr<Allocator> in_nestedAllocator) :
    vsg::Allocator(std::move(in_nestedAllocator))
{
    size_t alignment = 4;
    size_t new_blockSize = size_t(1024) * size_t(1024) * size_t(1024);

    callocatorMemoryBlocks.resize(vsg::ALLOCATOR_AFFINITY_LAST);
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_OBJECTS].reset(new CustomMemoryBlock("ALLOCATOR_AFFINITY_OBJECTS", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_DATA].reset(new CustomMemoryBlock("ALLOCATOR_AFFINITY_DATA", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_NODES].reset(new CustomMemoryBlock("ALLOCATOR_AFFINITY_NODES", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_PHYSICS].reset(new CustomMemoryBlock("ALLOCATOR_AFFINITY_PHYSICS", new_blockSize, 16));

    if (debug()) std::cout << "CustomAllocator()" << this<<std::endl;
}

CustomAllocator::~CustomAllocator()
{
    if (debug()) std::cout << "~CustomAllocator() " << this << std::endl;
}

void CustomAllocator::report(std::ostream& out) const
{
    out << "CustomAllocator::report() " << callocatorMemoryBlocks.size() << std::endl;

    for (const auto& memoryBlock : callocatorMemoryBlocks)
    {
        if (memoryBlock) memoryBlock->report(out);
    }
}

void* CustomAllocator::allocate(std::size_t size, vsg::AllocatorAffinity allocatorAffinity)
{
    std::scoped_lock<std::mutex> lock(mutex);

    // create a MemoryBlocks entry if one doesn't already exist
    if (allocatorAffinity > callocatorMemoryBlocks.size())
    {
        size_t blockSize = 1024 * 1024; // Megabyte
        callocatorMemoryBlocks.resize(allocatorAffinity + 1);
        callocatorMemoryBlocks[allocatorAffinity].reset(new CustomMemoryBlock("CustomMemoryBlockAffinity", blockSize, default_alignment));
    }

    auto& memoryBlock = callocatorMemoryBlocks[allocatorAffinity];
    if (memoryBlock)
    {
        auto mem_ptr = memoryBlock->allocate(size);
        if (mem_ptr)
        {
            return mem_ptr;
        }
    }

    return operator new (size); //, std::align_val_t{default_alignment});
}

bool CustomAllocator::deallocate(void* ptr, std::size_t size)
{
    std::scoped_lock<std::mutex> lock(mutex);

    for (auto& memoryBlocks : callocatorMemoryBlocks)
    {
        if (memoryBlocks)
        {
            if (memoryBlocks->deallocate(ptr, size))
            {
                return true;
            }
        }
    }

    if (nestedAllocator && nestedAllocator->deallocate(ptr, size)) return true;

    operator delete (ptr);
    return true;
}

