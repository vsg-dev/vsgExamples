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

    size_t max_slot_size = (1 << 15) - 1;

    if (debug()) std::cout<<"    capacity = "<<capacity<<", max_slot_size = "<<max_slot_size<<std::endl;

    // set up the free tracking to encompass the whole buffer
    freeLists.emplace_back();
    FreeList& freeList = freeLists.front();
    freeList.minimum_size = 2 * sizeof(Element);
    freeList.maximum_size = (max_slot_size - 1) * sizeof(Element);
    freeList.head = 1; // start at position 1 so that position 0 can be used to mark beginning or end of free lists
    freeList.count = 0;

    // mark the first element as 0.
    memory[0].index = 0;

    size_t previous_position = 0; // 0 marks the beginning of the free list
    size_t position = freeList.head;
    for(; position < capacity;)
    {
        size_t next_position = std::min(position + max_slot_size, capacity);

        memory[position] = Element{ (previous_position == 0) ? 0 : (position - previous_position), next_position - position, 1 };
        memory[position+1].index = previous_position;
        memory[position+2].index = (next_position < capacity) ? next_position : 0;
        previous_position = position;
        position = next_position;
        ++freeList.count;
    }

    if (debug() && !validate())
    {
        report(std::cout);
        throw "Validation error";
    }

    if (debug())
    {
        report(std::cout);
    }
}

IntrusiveMemoryBlock::~IntrusiveMemoryBlock()
{
    //operator delete (memory, std::align_val_t{block_alignment});
    operator delete (memory);
}

bool IntrusiveMemoryBlock::freeSlotsAvaible(size_t size) const
{
    for(auto& freeList : freeLists)
    {
        if (freeList.maximum_size >= size && freeList.count > 0) return true;
    }
    return false;
}

#if 1
void* IntrusiveMemoryBlock::allocate(std::size_t size)
{
    for(auto& freeList : freeLists)
    {
        // check if freeList has available slots and maximum_size is big enough
        if (freeList.count == 0 || size > freeList.maximum_size) continue;

        size_t freePosition = freeList.head;
        while (freePosition < capacity)
        {
            auto& slot = memory[freePosition];
            if (slot.status != 1)
            {
                throw "Warning: allocated slot found in freeList";
            }

            size_t previousFreePosition = memory[freePosition+1].index;
            size_t nextFreePosition = memory[freePosition+2].index;

            size_t slotSpace = static_cast<size_t>(slot.next);
            size_t nextPosition = freePosition + slotSpace;
            size_t slotSize = sizeof(Element) * (slotSpace - 1);
            size_t minimumNumElementsInSlot = freeList.minimum_size / sizeof(Element);
            if (size <= slotSize)
            {
                // we can us slot for memory;
                size_t numElementsToBeUsed = (size + sizeof(Element) - 1) / sizeof(Element);
                if ((numElementsToBeUsed + minimumNumElementsInSlot) < slotSize)
                {
                    // enough space in slot to split, so adjust
                    size_t newSlotPosition = freePosition + 1 + numElementsToBeUsed;
                    slot.next = static_cast<Offset>(newSlotPosition - freePosition);

                    // set up the new slot as a free slot
                    auto& newSlot = memory[newSlotPosition] = Element(slot.next, static_cast<Offset>(nextPosition - newSlotPosition), 1);
                    memory[newSlotPosition+1] = previousFreePosition;
                    memory[newSlotPosition+2] = nextFreePosition;

                    if (previousFreePosition != 0)
                    {
                        // need to update the previous slot in the free list
                        memory[previousFreePosition+2].index = newSlotPosition; // set previous free slots next index to the newly created slot
                    }
                    else
                    {
                        memory[newSlotPosition+1] = 0;
                    }


                    if (nextFreePosition != 0)
                    {
                        // need to update the previous slot in the free list
                        memory[nextFreePosition+1].index = newSlotPosition; // set next free slots previous index to the newly created slot
                    }

                    if (nextPosition < capacity)
                    {
                        auto& nextSlot = memory[nextPosition];
                        nextSlot.previous = newSlot.next;
                    }

                    if (freePosition == freeList.head)
                    {
                        // slot was at head of freeList so move it to the new slot position
                        freeList.head = newSlotPosition;
                    }
                }
                else
                {
                    // not enough space to split up slot, so remove it from freeList
                }

                slot.status = 0; // mark slot as allocated
                return &memory[freePosition+1];
            }

            freePosition = nextFreePosition;
        }

    }

    return nullptr;
}
#else
void* IntrusiveMemoryBlock::allocate(std::size_t size)
{
    FreeList& freeList = freeLists.front();
    if (freeList.head < capacity)
    {
        size_t minimumSize = freeList.minimum_size / sizeof(Element);

        auto& slot = memory[freeList.head];
        size_t slotSpace = static_cast<size_t>(slot.next) - 1;
        if (size <= slotSpace * sizeof(Element))
        {

            size_t allocationStart = freeList.head+1;
            size_t alignedSize = (size + sizeof(value_type) - 1) / sizeof(value_type);

            if (alignedSize+minimumSize <= slotSpace)
            {
                size_t newStart = freeList.head + 1 + alignedSize;

                if (debug())
                {
                    std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" Allocated and splitting slot, orignal freeList.head = "<<freeList.head<<std::endl;
                    std::cout<<"    slot = {"<< slot.previous<<", "<<slot.next<<", "<<slot.status <<std::endl;
                    std::cout<<"    freeList.head = "<< freeList.head <<std::endl;
                    std::cout<<"    slotSpace = "<< slotSpace <<std::endl;
                    std::cout<<"    alignedSize = "<< alignedSize<<std::endl;
                    std::cout<<"    newStart = "<< newStart <<std::endl;
                    std::cout<<"    slot.next = "<< slot.next <<std::endl;
                }

                size_t nextSlotStart = freeList.head + slot.next;

                // set up remaining space as free space
                memory[newStart] = memory[newStart+1] = Element{newStart-freeList.head, freeList.head + slot.next - newStart, 1};
                slot = Element{slot.previous, newStart-freeList.head, 0};

                if (nextSlotStart < capacity)
                {
                    auto& next_slot = memory[nextSlotStart];
                    next_slot.previous = nextSlotStart - newStart;
                    if (debug()) std::cout<<"   next_slot.previous = "<<next_slot.previous<<std::endl;
                }

                freeList.head = newStart;

                if (debug()) std::cout<<"   final freeList.head = "<<freeList.head<<std::endl;
            }
            else
            {
                // not enough free space to move freeList.head to next location.
                freeList.head += slot.next;
                --freeList.count;

                slot = Element{slot.previous, slot.next, 0};

                if (debug()) std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" Allocated but not big enough to split so advancing to : "<<freeList.head<<std::endl;
            }
            if (debug() && !validate())
            {
                report(std::cout);
                throw "Validation error";
            }
            return &(memory[allocationStart]);
        }
    }

    if (debug()) std::cout<<"IntrusiveMemoryBlock::allocate("<<size<<") "<<this<<" no space left. freeList.head = "<<freeList.head<<", "<<capacity<<std::endl;

    return nullptr;
}
#endif

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

    size_t position = 1;
    while(position < capacity)
    {
        auto& slot = memory[position];
        if (slot.status == 1)
        {
            std::cout<<"   memory["<<position<<"] slot { "<<slot.previous<<", "<<slot.next<<", "<<slot.status<<"}, "<<memory[position+1].index<<", "<<memory[position+2].index<<std::endl;
        }
        else
        {
            std::cout<<"   memory["<<position<<"] slot { "<<slot.previous<<", "<<slot.next<<", "<<slot.status<<"} "<<std::endl;
        }

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

