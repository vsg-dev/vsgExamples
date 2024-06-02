#include "IntrusiveAllocator.h"

#include <iostream>
#include <algorithm>
#include <cstddef>

using namespace experimental;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IntrusiveMemoryBlock
//
IntrusiveMemoryBlock::IntrusiveMemoryBlock(const std::string& in_name, size_t in_blockSize, size_t in_alignment) :
    name(in_name),
    alignment(in_alignment),
    blockSize(in_blockSize)
{
    if (debug()) std::cout<<"IntrusiveMemoryBlock::IntrusiveMemoryBlock("<<in_blockSize<<", "<<in_alignment<<")"<<std::endl;

    alignment = std::max(alignment, sizeof(value_type)); // we need to be a multiple of sizeof(value_type)
    block_alignment = std::max(alignment, alignof(std::max_align_t));
    block_alignment = std::max(block_alignment, size_t{16});

    // round blockSize up to nearest aligned size
    blockSize = ((blockSize+alignment-1) / alignment) * alignment;

    //memory = static_cast<uint8_t*>(operator new (blockSize, std::align_val_t{block_alignment}));
    memory = static_cast<value_type*>(operator new (blockSize));
    memory_end = memory + blockSize;
    capacity = blockSize / alignment;

    // set up the free tracking to encompass the whole buffer
    firstFree = 0;
    memory[firstFree] = capacity;
    memory[firstFree+1] = capacity;

    if (debug()) std::cout<<"   firstFree = "<<firstFree<<", capacity = "<<capacity<<std::endl;
}

IntrusiveMemoryBlock::~IntrusiveMemoryBlock()
{
    //operator delete (memory, std::align_val_t{block_alignment});
    operator delete (memory);
}

void* IntrusiveMemoryBlock::allocate(std::size_t size)
{
    if (firstFree < capacity)
    {
        value_type slotEnd = memory[firstFree];
        value_type slotSize = ((slotEnd-firstFree)-1) * sizeof(value_type);
        if (size <= slotSize)
        {
            value_type nextFree = memory[firstFree+1];

            value_type allocationStart = firstFree+1;
            value_type allocationEnd = allocationStart + (size + sizeof(value_type) - 1)/ sizeof(value_type);

            if ((allocationEnd + 2) < slotEnd)
            {
                memory[firstFree] = allocationEnd;
                memory[allocationEnd] = slotEnd;
                memory[allocationEnd+1] = nextFree;
                firstFree = allocationEnd;

                if (debug()) std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" Allocated and splitting slot "<<std::endl;

                return &memory[allocationStart];
            }
            else
            {
                // not enough space left in slot to justify creating a free slot
                firstFree = nextFree;

                if (debug()) std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" Allocated but not big enough to split so advancing to : "<<nextFree<<std::endl;

                return &memory[allocationStart];
            }
        }
    }

    if (debug()) std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" no space left. firstFree = "<<firstFree<<", "<<capacity<<std::endl;
    return nullptr;
}

bool IntrusiveMemoryBlock::deallocate(void* ptr, std::size_t size)
{
    if (within(ptr))
    {
        if (debug()) std::cout<<"IntrusiveMemoryBlock::deallocate(("<<ptr<<", "<<size<<") TODO implement free "<<std::endl;
        return true;
    }

    return false;
}

void IntrusiveMemoryBlock::report(std::ostream& out) const
{
    out << "MemoryBlock "<<this<<" "<<name<<std::endl;
    out << "    alignment = "<<alignment<<std::endl;
    out << "    block_alignment = "<<block_alignment<<std::endl;
    out << "    blockSize = "<<blockSize<<", memory = "<<static_cast<void*>(memory)<<std::endl;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IntrusiveMemoryBlocks
//
IntrusiveMemoryBlocks::IntrusiveMemoryBlocks(IntrusiveAllocator* in_parent, const std::string& in_name, size_t in_blockSize, size_t in_alignment) :
    parent(in_parent),
    name(in_name),
    alignment(in_alignment),
    blockSize(in_blockSize)
{
}

IntrusiveMemoryBlocks::~IntrusiveMemoryBlocks()
{
}

void* IntrusiveMemoryBlocks::allocate(std::size_t size)
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

    auto block = std::make_shared<IntrusiveMemoryBlock>(name, new_blockSize, alignment);

    if (parent)
    {
        parent->memoryBlocks[block->memory] = block;
    }

    //memoryBlockWithSpace = block;

    auto ptr = block->allocate(size);

    memoryBlocks[block->memory] = std::move(block);

    return ptr;
}

bool IntrusiveMemoryBlocks::deallocate(void* ptr, std::size_t size)
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

void IntrusiveMemoryBlocks::report(std::ostream& out) const
{
    std::cout<<"\nIntrusiveMemoryBlocks::report() memoryBlocks.size() = "<<memoryBlocks.size()<<std::endl;
    for(auto& memoryBlock : memoryBlocks)
    {
        memoryBlock.second->report(out);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IntrusiveAllocator
//
IntrusiveAllocator::IntrusiveAllocator(std::unique_ptr<Allocator> in_nestedAllocator) :
    vsg::Allocator(std::move(in_nestedAllocator))
{
    default_alignment = 4;

    size_t Megabyte = size_t(1024) * size_t(1024);
#if 0
    size_t new_blockSize = size_t(1024) * Megabyte;
#else
    size_t new_blockSize = size_t(16) * Megabyte;
#endif

    callocatorMemoryBlocks.resize(vsg::ALLOCATOR_AFFINITY_LAST);
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_OBJECTS].reset(new IntrusiveMemoryBlocks(this, "ALLOCATOR_AFFINITY_OBJECTS", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_DATA].reset(new IntrusiveMemoryBlocks(this, "ALLOCATOR_AFFINITY_DATA", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_NODES].reset(new IntrusiveMemoryBlocks(this, "ALLOCATOR_AFFINITY_NODES", new_blockSize, default_alignment));
    callocatorMemoryBlocks[vsg::ALLOCATOR_AFFINITY_PHYSICS].reset(new IntrusiveMemoryBlocks(this, "ALLOCATOR_AFFINITY_PHYSICS", new_blockSize, 16));

    if (debug()) std::cout << "IntrusiveAllocator()" << this<<std::endl;
}

IntrusiveAllocator::~IntrusiveAllocator()
{
    if (debug()) std::cout << "~IntrusiveAllocator() " << this << std::endl;
}

void IntrusiveAllocator::report(std::ostream& out) const
{
    out << "IntrusiveAllocator::report() " << callocatorMemoryBlocks.size() << std::endl;

    for (const auto& memoryBlock : callocatorMemoryBlocks)
    {
        if (memoryBlock) memoryBlock->report(out);
    }
}

void* IntrusiveAllocator::allocate(std::size_t size, vsg::AllocatorAffinity allocatorAffinity)
{
    std::scoped_lock<std::mutex> lock(mutex);

    // create a MemoryBlocks entry if one doesn't already exist
    if (allocatorAffinity > callocatorMemoryBlocks.size())
    {
        size_t blockSize = 1024 * 1024; // Megabyte
        callocatorMemoryBlocks.resize(allocatorAffinity + 1);
        callocatorMemoryBlocks[allocatorAffinity].reset(new IntrusiveMemoryBlocks(this, "IntrusiveMemoryBlockAffinity", blockSize, default_alignment));
    }

    auto& memoryBlock = callocatorMemoryBlocks[allocatorAffinity];
    if (memoryBlock)
    {
        auto mem_ptr = memoryBlock->allocate(size);
        if (mem_ptr)
        {
            if (debug()) std::cout<<"1 IntrusiveAllocator::allocate("<<size<<", "<<allocatorAffinity<<") ptr = "<<mem_ptr<<std::endl;
            return mem_ptr;
        }
    }

    if (debug()) std::cout<<"2 Fall through IntrusiveAllocator::allocate("<<size<<", "<<allocatorAffinity<<")"<<std::endl;

    return operator new (size); //, std::align_val_t{default_alignment});
}

bool IntrusiveAllocator::deallocate(void* ptr, std::size_t size)
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

