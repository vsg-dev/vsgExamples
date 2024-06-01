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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CustomMemorySlots
//
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

//    for(auto itr = availableSlots.rbegin(); itr != availableSlots.rend(); ++itr)
    for(size_t i = availableSlots.size(); i > 0; --i)
    {
        //Index slotIndex = *itr;
        Index slotIndex = availableSlots[i-1];
        Offset slotSize = offsets[slotIndex+1] - offsets[slotIndex];
        if (size == slotSize)
        {
            //availableSlots.erase(itr.base());
            availableSlots.erase(availableSlots.begin()+(i-1));
            if (debug()) std::cout<<"CustomMemorySlots::reserve("<<size<<") complete reserve, slotIndex = "<<slotIndex<<std::endl;
            return {true, offsets[slotIndex]};
        }
        else if (size < slotSize)
        {
            Offset start = offsets[slotIndex];
            Offset aligned_end = ((start + size + alignment-1) / alignment) * alignment;

            if (aligned_end < offsets[slotIndex+1])
            {
                if (debug())
                {
                    std::cout<<"CustomMemorySlots::reserve("<<size<<") Partial reserve, slotIndex = "<<slotIndex<<", alignment = "<<alignment<<std::endl;
                    std::cout<<"    start = "<<start<<", original end = "<<start + size<<", alignmed end = "<<aligned_end<<std::endl;
                    std::cout<<"    offsets.size() = "<<offsets.size()<<", slotIndex+1 = "<<slotIndex+1<<std::endl;
                }

                offsets.insert(offsets.begin() + slotIndex + 1, aligned_end);

                //(*itr) = slotIndex+1;
                availableSlots[i-1] = slotIndex+1;
                return {true, start};
            }
            else
            {
                //availableSlots.erase(itr.base());
                availableSlots.erase(availableSlots.begin()+(i-1));
                if (debug()) std::cout<<"CustomMemorySlots::reserve("<<size<<") complete reserve2, slotIndex = "<<slotIndex<<std::endl;
                return {true, start};

            }

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CustomMemoryBlock
//
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

    return false;
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CustomMemoryBlocks
//
CustomMemoryBlocks::CustomMemoryBlocks(CustomAllocator* in_parent, const std::string& in_name, size_t in_blockSize, size_t in_alignment) :
    parent(in_parent),
    name(in_name),
    alignment(in_alignment),
    blockSize(in_blockSize)
{
}

CustomMemoryBlocks::~CustomMemoryBlocks()
{
}

void* CustomMemoryBlocks::allocate(std::size_t size)
{
    if (memoryBlockWithSpace)
    {
        auto ptr = memoryBlockWithSpace->allocate(size);
        if (ptr) return ptr;
    }

    size_t new_blockSize = std::max(size, blockSize);
    for (auto itr = memoryBlocks.begin(); itr != memoryBlocks.end(); ++itr)
    {
        auto& block = itr->second;
        if (block != memoryBlockWithSpace)
        {
            auto ptr = block->allocate(size);
            if (ptr) return ptr;
        }
    }

    auto block = std::make_shared<CustomMemoryBlock>(name, new_blockSize, alignment);

    if (parent)
    {
        parent->memoryBlocks[block->memory] = block;
    }

    //memoryBlockWithSpace = block;

    auto ptr = block->allocate(size);

    memoryBlocks[block->memory] = std::move(block);

    return ptr;
}

bool CustomMemoryBlocks::deallocate(void* ptr, std::size_t size)
{
#if 0
    return memoryBlock->deallocate(ptr, size);
#else
    for(auto& memoryBlock : memoryBlocks)
    {
        if (memoryBlock.second->deallocate(ptr, size))
        {
            return true;
        }
    }

    return false;
#endif
}

void CustomMemoryBlocks::report(std::ostream& out) const
{
    std::cout<<"\nCustomMemoryBlocks::report() memoryBlocks.size() = "<<memoryBlocks.size()<<std::endl;
    for(auto& memoryBlock : memoryBlocks)
    {
        memoryBlock.second->report(out);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CustomAllocator
//
CustomAllocator::CustomAllocator(std::unique_ptr<Allocator> in_nestedAllocator) :
    vsg::Allocator(std::move(in_nestedAllocator))
{
    default_alignment = 4;

    size_t Megabyte = size_t(1024) * size_t(1024);
#if 0
    size_t new_blockSize = size_t(1024) * Megabyte;
#else
    size_t new_blockSize = size_t(32) * Megabyte;
#endif

    callocatorMemoryBlocks.resize(vsg::ALLOCATOR_AFFINITY_LAST);
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_OBJECTS].reset(new CustomMemoryBlocks(this, "ALLOCATOR_AFFINITY_OBJECTS", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_DATA].reset(new CustomMemoryBlocks(this, "ALLOCATOR_AFFINITY_DATA", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_NODES].reset(new CustomMemoryBlocks(this, "ALLOCATOR_AFFINITY_NODES", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_PHYSICS].reset(new CustomMemoryBlocks(this, "ALLOCATOR_AFFINITY_PHYSICS", new_blockSize, 16));

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
        callocatorMemoryBlocks[allocatorAffinity].reset(new CustomMemoryBlocks(this, "CustomMemoryBlockAffinity", blockSize, default_alignment));
    }

    auto& memoryBlock = callocatorMemoryBlocks[allocatorAffinity];
    if (memoryBlock)
    {
        auto mem_ptr = memoryBlock->allocate(size);
        if (mem_ptr)
        {
            if (debug()) std::cout<<"1 CustomAllocator::allocate("<<size<<", "<<allocatorAffinity<<") ptr = "<<mem_ptr<<std::endl;
            return mem_ptr;
        }
    }

    if (debug()) std::cout<<"2 Fall through CustomAllocator::allocate("<<size<<", "<<allocatorAffinity<<")"<<std::endl;

    return operator new (size); //, std::align_val_t{default_alignment});
}

bool CustomAllocator::deallocate(void* ptr, std::size_t size)
{
    std::scoped_lock<std::mutex> lock(mutex);

#if 0

    for (auto& memoryBlocks : callocatorMemoryBlocks)
    {
        if (memoryBlocks && memoryBlocks->deallocate(ptr, size))
        {
            return true;
        }
    }
#else
    if (memoryBlocks.empty()) return false;

    auto itr = memoryBlocks.upper_bound(ptr);
    if (itr != memoryBlocks.end())
    {
        if (itr != memoryBlocks.begin())
        {
            --itr;
            auto& block = itr->second;
            if (block->deallocate(ptr, size))
            {
                if (debug()) std::cout<<"A Allocator::deallocate("<<ptr<<", "<<size<<") memory = "<<itr->first<<std::endl;
                return true;
            }
            else
            {
                if (debug()) std::cout<<"B failed Allocator::deallocate("<<ptr<<", "<<size<<") memory = "<<itr->first<<std::endl;
            }
        }
        else
        {
            auto& block = itr->second;
            if (block->deallocate(ptr, size))
            {
                if (debug()) std::cout<<"C Allocator::deallocate("<<ptr<<", "<<size<<") memory = "<<itr->first<<std::endl;
                return true;
            }
            else
            {
                if (debug()) std::cout<<"D failed Allocator::deallocate("<<ptr<<", "<<size<<") memory = "<<itr->first<<std::endl;
            }
        }
    }
    else
    {
        auto& block = memoryBlocks.rbegin()->second;
        if (block->deallocate(ptr, size))
        {
            if (debug()) std::cout<<"E Allocator::deallocate("<<ptr<<", "<<size<<") memoryBlocks.rbegin()->first = "<<memoryBlocks.rbegin()->first<<std::endl;
            return true;
        }
        else
        {
            if (debug()) std::cout<<"F failed Allocator::deallocate("<<ptr<<", "<<size<<") memoryBlocks.rbegin()->first = "<<memoryBlocks.rbegin()->first<<std::endl;
        }
    }


#endif


    if (nestedAllocator && nestedAllocator->deallocate(ptr, size))
    {
        if (debug()) std::cout<<"G Fall through nestedAllocator->deallocate("<<ptr<<", "<<size<<")"<<std::endl;
        return true;
    }

    if (debug()) std::cout<<"H Fall through"<<std::endl;\

    operator delete (ptr);
    return true;
}

