#include "IntrusiveAllocator.h"

#include <vsg/io/Logger.h>

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
    memory_end = memory + blockSize / sizeof(value_type);
    capacity = blockSize / alignment;

    // set up the free tracking to encompass the whole buffer
    firstFree = 0;

    // fill in free list

    size_t max_slot_size = (1 << 15) - 1;
    size_t previous_position = 0;

    size_t position = 0;

    if (debug()) std::cout<<"    capacity = "<<capacity<<", max_slot_size = "<<max_slot_size<<std::endl;

    for(; position < capacity;)
    {
        size_t next_position = std::min(position + max_slot_size, capacity);

        Element element { position - previous_position,
                          next_position - position,
                          1 };

        memory[position] = element;
        previous_position = position;
        position = next_position;
    }

    if (debug() && !validate())
    {
        report(std::cout);
        throw "Validation error";
    }

    if (debug())
    {
        std::cout<<"memory = "<<memory<<", memory_end ="<<memory_end<<", max slot memory size = "<<max_slot_size*sizeof(Element)<<std::endl;

        size_t num_slots = 0;
        position = firstFree;
        for(;;)
        {
            std::cout<<"position = "<<position<<", { "<<memory[position].previous<<", "<<memory[position].next<<", "<<memory[position].status<<"}"<<std::endl;
            ++num_slots;
            position += memory[position].next;
            if (position>=capacity) break;
        }

        std::cout<<"num_slots = "<<num_slots<<std::endl;
    }
}

IntrusiveMemoryBlock::~IntrusiveMemoryBlock()
{
    //operator delete (memory, std::align_val_t{block_alignment});
    operator delete (memory);
}

void* IntrusiveMemoryBlock::allocate(std::size_t size)
{
    size_t minimumSize = 4;

    if (firstFree < capacity)
    {
        auto& slot = memory[firstFree];
        size_t slotSpace = static_cast<size_t>(slot.next) - 1;
        if (size <= slotSpace * sizeof(Element))
        {

            size_t allocationStart = firstFree+1;
            size_t alignedSize = (size + sizeof(value_type) - 1) / sizeof(value_type);

            if (alignedSize+minimumSize <= slotSpace)
            {
                size_t newStart = firstFree + 1 + alignedSize;

                if (debug())
                {
                    std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" Allocated and splitting slot, orignal firstFree = "<<firstFree<<std::endl;
                    std::cout<<"    slot = {"<< slot.previous<<", "<<slot.next<<", "<<slot.status <<std::endl;
                    std::cout<<"    firstFree = "<< firstFree <<std::endl;
                    std::cout<<"    slotSpace = "<< slotSpace <<std::endl;
                    std::cout<<"    alignedSize = "<< alignedSize<<std::endl;
                    std::cout<<"    newStart = "<< newStart <<std::endl;
                    std::cout<<"    slot.next = "<< slot.next <<std::endl;
                }

                size_t nextSlotStart = firstFree + slot.next;

                // set up remaining space as free space
                memory[newStart] = memory[newStart+1] = Element{newStart-firstFree, firstFree + slot.next - newStart, 1};
                slot = Element{slot.previous, newStart-firstFree, 0};

                if (nextSlotStart < capacity)
                {
                    auto& next_slot = memory[nextSlotStart];
                    next_slot.previous = nextSlotStart - newStart;
                    if (debug()) std::cout<<"   next_slot.previous = "<<next_slot.previous<<std::endl;
                }

                firstFree = newStart;

                if (debug()) std::cout<<"   final firstFree = "<<firstFree<<std::endl;
            }
            else
            {
                // not enough free space to move firstFree to next location.
                firstFree += slot.next;

                slot = Element{slot.previous, slot.next, 0};

                if (debug()) std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" Allocated but not big enough to split so advancing to : "<<firstFree<<std::endl;
            }
            if (debug() && !validate())
            {
                report(std::cout);
                throw "Validation error";
            }
            return &(memory[allocationStart]);
        }
    }

    if (debug()) std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" no space left. firstFree = "<<firstFree<<", "<<capacity<<std::endl;

    return nullptr;
}

bool IntrusiveMemoryBlock::deallocate(void* ptr, std::size_t size)
{
    if (within(ptr))
    {
        size_t slotStart = static_cast<size_t>(static_cast<Element*>(ptr) - memory);

        auto& slot = memory[slotStart-1];

        if (debug()) std::cout<<"IntrusiveMemoryBlock::deallocate(("<<ptr<<", "<<size<<") slotStart =  "<<slotStart<<", slot = { "<<slot.previous<<" , "<<slot.next<<", "<<slot.status<<"}"<<std::endl;

        if (debug() && !validate())
        {
            report(std::cout);
            throw "Validation error";
        }

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

    size_t position = 0;
    while(position < capacity)
    {
        auto& slot = memory[position];
        std::cout<<"   slot { "<<slot.previous<<", "<<slot.next<<", "<<slot.status<<"}"<<std::endl;
        position += slot.next;
        if (slot.next == 0) break;
    }

}

bool IntrusiveMemoryBlock::validate() const
{
    size_t previous = 0;
    size_t position = 0;
    while(position < capacity)
    {
        auto& slot = memory[position];
        if (slot.previous > capacity || slot.next > capacity)
        {
            std::cout<<"Warning: slot.corrupted invalid position = "<<position<<", slot = {"<<slot.previous<<", "<<slot.next<<", "<<slot.status<<"}"<<std::endl;
            return false;
        }

        if (slot.previous != 0)
        {
            if (slot.previous > position)
            {
                std::cout<<"Warning: slot.previous invalid position = "<<position<<", slot = {"<<slot.previous<<", "<<slot.next<<", "<<slot.status<<"}"<<std::endl;
                return false;
            }
            size_t previous_position = position - slot.previous;
            if (previous_position != previous)
            {
                std::cout<<"Warning: previous slot = "<<previous<<" doesn't match slot.previous, position = "<<position<<", slot = {"<<slot.previous<<", "<<slot.next<<", "<<slot.status<<"}"<<std::endl;
                return false;
            }
        }

        previous = position;
        position += slot.next;
        if (slot.next == 0) break;
    }

    // std::cout<<"No invalid entries found"<<std::endl;

    return true;
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
    for (auto& block : memoryBlocks)
    {
        if (block != memoryBlockWithSpace)
        {
            auto ptr = block->allocate(size);
            if (ptr) return ptr;
        }
    }

    auto new_block = std::make_shared<IntrusiveMemoryBlock>(name, new_blockSize, alignment);

    if (parent)
    {
        parent->memoryBlocks[new_block->memory] = new_block;
    }

    memoryBlockWithSpace = new_block;
    memoryBlocks.push_back(new_block);

    auto ptr = new_block->allocate(size);

    return ptr;
}

void IntrusiveMemoryBlocks::report(std::ostream& out) const
{
    std::cout<<"\nIntrusiveMemoryBlocks::report() memoryBlocks.size() = "<<memoryBlocks.size()<<std::endl;
    for(auto& memoryBlock : memoryBlocks)
    {
        memoryBlock->report(out);
    }
}

bool IntrusiveMemoryBlocks::validate() const
{
    bool valid = true;
    for(auto& memoryBlock : memoryBlocks)
    {
        valid = memoryBlock->validate() && valid ;
    }
    return valid;
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
    size_t new_blockSize = size_t(1) * Megabyte;
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

void IntrusiveAllocator::setBlockSize(vsg::AllocatorAffinity allocatorAffinity, size_t blockSize)
{
   std::scoped_lock<std::mutex> lock(mutex);

    if (size_t(allocatorAffinity) < callocatorMemoryBlocks.size())
    {
        callocatorMemoryBlocks[allocatorAffinity]->blockSize = blockSize;
    }
    else
    {
        auto name = vsg::make_string("MemoryBlocks_", allocatorAffinity);

        callocatorMemoryBlocks.resize(allocatorAffinity + 1);
        callocatorMemoryBlocks[allocatorAffinity].reset(new IntrusiveMemoryBlocks(this, name, blockSize, default_alignment));
    }
}

void IntrusiveAllocator::report(std::ostream& out) const
{
    out << "IntrusiveAllocator::report() " << callocatorMemoryBlocks.size() << std::endl;

    for (const auto& memoryBlock : callocatorMemoryBlocks)
    {
        if (memoryBlock) memoryBlock->report(out);
    }

    validate();
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

bool IntrusiveAllocator::validate() const
{
    bool valid = true;
    for(auto& memoryBlock : callocatorMemoryBlocks)
    {
        valid = memoryBlock->validate() && valid ;
    }
    return valid;
}

