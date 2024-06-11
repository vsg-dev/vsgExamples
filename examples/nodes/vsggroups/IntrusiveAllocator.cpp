#include "IntrusiveAllocator.h"

#include <vsg/io/Logger.h>

#include <iostream>
#include <algorithm>
#include <cstddef>

using namespace vsg;

bool& vsg::debug()
{
    static bool s_debug = false;
    return s_debug;
}

#if 0
#define DEBUG_MESSAGE if (debug()) std::cout
#else
#define DEBUG_MESSAGE if (false) std::cout
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IntrusiveMemoryBlock
//
IntrusiveMemoryBlock::IntrusiveMemoryBlock(const std::string& in_name, size_t in_blockSize, size_t in_alignment) :
    name(in_name),
    alignment(in_alignment),
    blockSize(in_blockSize)
{
    DEBUG_MESSAGE<<"IntrusiveMemoryBlock::IntrusiveMemoryBlock("<<in_blockSize<<", "<<in_alignment<<")"<<std::endl;

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

    DEBUG_MESSAGE<<"    capacity = "<<capacity<<", max_slot_size = "<<max_slot_size<<std::endl;

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

void* IntrusiveMemoryBlock::allocate(std::size_t size)
{
    for(auto& freeList : freeLists)
    {
        // check if freeList has available slots and maximum_size is big enough
        if (freeList.count == 0 || size > freeList.maximum_size) continue;

        if (freeList.head == 0)
        {
            report(std::cout);

            std::cout<<"Warning:: Freelist has count "<<freeList.count<<", but head = "<<freeList.head<<std::endl;

            throw "Warning: allocated slot found in freeList";
        }

        size_t freePosition = freeList.head;
        while (freePosition != 0)
        {
            auto& slot = memory[freePosition];
            if (slot.status != 1)
            {
                report(std::cout);

                std::cout<<"Warning: allocated slot found in freeList, freePosition = "<<freePosition<<", slot{ "<<slot.previous<< ", "<<slot.next<<", "<<slot.status<<" }"<<std::endl;

                throw "Warning: allocated slot found in freeList";
            }

            size_t previousFreePosition = memory[freePosition+1].index;
            size_t nextFreePosition = memory[freePosition+2].index;

            if ((previousFreePosition > freePosition) || (nextFreePosition != 0 && freePosition > nextFreePosition))
            {
                report(std::cout);

                std::cout<<"Something topysyturvy has happened, previousFreePosition = "<<previousFreePosition<<", freePosition = "<<freePosition<<", nextFreePosition = "<<nextFreePosition<<std::endl;

                throw "Warning: topsyturvy";
            }

            size_t slotSpace = static_cast<size_t>(slot.next);
            size_t nextPosition = freePosition + slotSpace;
            size_t slotSize = sizeof(Element) * (slotSpace - 1);
            size_t minimumNumElementsInSlot = 1 + freeList.minimum_size / sizeof(Element);
            if (size <= slotSize)
            {
                // we can us slot for memory;
                size_t numElementsToBeUsed = (size + sizeof(Element) - 1) / sizeof(Element);
                if ((numElementsToBeUsed + minimumNumElementsInSlot) < slotSpace)
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
                    //std::cout<<"split slot "<<freePosition<<", "<<slotSize<<", "<<std::endl;
                }
                else
                {

                    // std::cout<<"Removing slot as it's fully used freePosition = "<<freePosition<<", previousFreePosition = "<<previousFreePosition<<", nextFreePosition = "<<nextFreePosition<<std::endl;

                    // not enough space to split up slot, so remove it from freeList
                    if (previousFreePosition != 0)
                    {
                        // need to update the previous slot in the free list
                        memory[previousFreePosition+2].index = nextFreePosition;
                    }


                    if (nextFreePosition != 0)
                    {
                        // need to update the previous slot in the free list
                        memory[nextFreePosition+1].index = previousFreePosition;
                    }

                    if (freePosition == freeList.head)
                    {
                        // slot was at head of freeList so move it to the new slot position
                        freeList.head = nextFreePosition;
                    }

                    // one list free slot availalbe
                    --freeList.count;

                    //std::cout<<"not enough space to split slot "<<freePosition<<std::endl;


                }

                slot.status = 0; // mark slot as allocated

                DEBUG_MESSAGE<<"IntrusiveMemoryBlock::allocate("<<size<<") slot used = "<<freePosition<<", "<<&memory[freePosition+1]<<std::endl;

                return &memory[freePosition+1];
            }

            //std::cout<<"Moving to next free slot "<<nextFreePosition<<std::endl;

            freePosition = nextFreePosition;
        }

    }

    return nullptr;
}

bool IntrusiveMemoryBlock::deallocate(void* ptr, std::size_t size)
{
    if (within(ptr))
    {
        auto& freeList = freeLists.front();
        size_t maxSize = 1 + freeList.maximum_size / sizeof(Element);

        //
        // sequential slots around the slot to be deallocated are named:
        //    PP (Previous' Previous), P (Previous), C (Current slot being deallocated), N (Next), NN (Next's Next)
        //
        // the FreeList linked list entries of interest are named:
        //    PPF (Previous' Previous Free), PNF (Previous's Next Free), NPF (Next's Previous Free), NNF (Next's Next Free)
        //


        size_t C = static_cast<size_t>(static_cast<Element*>(ptr) - memory) - 1;
        auto& slot = memory[C];

        DEBUG_MESSAGE<<"IntrusiveMemoryBlock::deallocate(("<<ptr<<", "<<size<<") C =  "<<C<<", slot = { "<<slot.previous<<" , "<<slot.next<<", "<<slot.status<<"}"<<std::endl;

        if (debug() && !validate())
        {
            report(std::cout);
            throw "Validation error";
        }

        if (slot.status != 0)
        {
            report(std::cout);
            throw "Attempt to deallocatoe already available slot";
        }

        // make slot as available
        slot.status = 1;

        // set up the indices for the previous and next slots
        size_t P = (slot.previous > 0) ? (C - static_cast<size_t>(slot.previous)) : 0;
        size_t N = C + static_cast<size_t>(slot.next);
        if (N >= capacity) N = 0;

        // set up the indices for the previous free entry
        size_t PNF = 0;
        if (P != 0)
        {
            if (memory[P].status != 0)
            {
                PNF = memory[P+2].index;
            }
        }

        // set up the indices for the next free entries
        size_t NN = 0;
        size_t NPF = 0;
        size_t NNF = 0;
        if (N != 0)
        {
            NN = N + static_cast<size_t>(memory[N].next);
            if (memory[N].status != 0)
            {
                NPF = memory[N + 1].index;
                NNF = memory[N + 2].index;
            }
        }

        // 3 way merge of P, C and C
        auto mergePCN = [&]() -> void
        {
            DEBUG_MESSAGE<<"   mergePCN(), P = "<<P<<", C = "<<C<<", N = "<<N<<std::endl;

            // update slots for the merge
            memory[P].next += memory[C].next + memory[N].next;
            if (NN != 0) memory[NN].previous = memory[P].next;

            // update freeList linked list entries
            if (PNF == N) // also implies NPF == P
            {
                // case 1. in order sequential
                DEBUG_MESSAGE<<"       case 1. in order sequential"<<std::endl;

                memory[P + 2].index = NNF;
                if (NNF != 0) memory[NNF + 1].index = P;
            }
            else if (PNF == N) // also implies NNF == P
            {
                // case 2. reverse sequential
                DEBUG_MESSAGE<<"       case 2. reverse sequential"<<std::endl;

                memory[P + 2].index = NNF;
                if (NPF != 0) memory[NPF + 1] = P;
            }
            else // P and N aren't directly connected within the freeList
            {
                // case 3. out of order
                DEBUG_MESSAGE<<"       case 3. out of order"<<std::endl;
                if (NPF != 0) memory[NPF + 2].index = NNF;
                if (NNF != 0) memory[NNF + 1].index = NPF;
            }

            // if N was the head then change head to P
            if (freeList.head == N) freeList.head = P;

            // N slot is nolonger a seperate free slot so decrement free count
            --freeList.count;

            if (debug()) report(std::cout);
        };

        // 2 way merge of P and C
        auto mergePC = [&]() -> void
        {
            DEBUG_MESSAGE<<"    mergePC(), P = "<<P<<", C = "<<C<<", N = "<<N<<std::endl;

            // update slots for the merge
            memory[P].next += memory[C].next;
            if (N != 0) memory[N].previous = memory[P].next;

            // freeList linked list entries will not need updating.

            if (debug()) report(std::cout);
        };

        // 2 way merge of C and N
        auto mergeCN = [&]() -> void
        {
            DEBUG_MESSAGE<<"    mergeCN(), P = "<<P<<", C = "<<C<<", N = "<<N<<", NN = "<<NN<<", NPF ="<<NPF<<", NNF = "<<NNF<<std::endl;

            // update slots for merge
            memory[C].next += memory[N].next;
            if (NN != 0) memory[NN].previous = memory[C].next;

            // update freeList linked list entries
            if (NPF != 0) memory[NPF + 2].index = C;
            if (NNF != 0) memory[NNF + 1].index = C;
            memory[C + 1].index = NPF;
            memory[C + 2].index = NNF;

            // if N was the head then change head to C
            if (freeList.head == N) freeList.head = C;

            if (debug()) report(std::cout);
        };

        // standalone insertion of C into head of freeList
        auto standalone = [&]() -> void
        {
            DEBUG_MESSAGE<<"    standalone(), P = "<<P<<", C = "<<C<<", N = "<<N<<std::endl;
            memory[C + 1].index = 0;
            memory[C + 2].index = freeList.head;

            if (freeList.head != 0)
            {
                memory[freeList.head + 1] = C;  // set previous heads previousFree to C.
            }

            // set the head to C.
            freeList.head = C;

            // Inserted new free slot so increment free count
            ++freeList.count;

            if (debug()) report(std::cout);
        };

        if (P != 0 && memory[P].status != 0)
        {
            if (N != 0 && memory[N].status != 0)
            {
                if ((static_cast<size_t>(memory[P].next) + static_cast<size_t>(memory[C].next) + static_cast<size_t>(memory[N].next)) <= maxSize) mergePCN();
                else if ((static_cast<size_t>(memory[P].next) + static_cast<size_t>(memory[C].next)) <= maxSize) mergePC(); // merge P and C
                else if ((static_cast<size_t>(memory[C].next) + static_cast<size_t>(memory[N].next)) <= maxSize) mergeCN(); // merge C and N
                else standalone(); // C is standalone
            }
            else if ((static_cast<size_t>(memory[P].next) + static_cast<size_t>(memory[C].next)) <= maxSize) mergePC(); // merge P and C
            else standalone(); // C is standalone
        }
        else if (N != 0 && memory[N].status != 0)
        {
            if (static_cast<size_t>(memory[C].next) + static_cast<size_t>(memory[N].next) <= maxSize) mergeCN(); // merge C and N
            else standalone(); // standalone
        }
        else
        {
            // C is standalone
            standalone();
        }

        return true;
    }

    DEBUG_MESSAGE<<"IntrusiveMemoryBlock::deallocate(("<<ptr<<", "<<size<<") OUTWITH block : "<<this<<std::endl;

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

    std::cout<<"   freeList.size() = "<<freeLists.size()<<" { "<<std::endl;
    for(auto& freeList : freeLists)
    {
        std::cout<<"   FreeList ( minimum_size = "<<freeList.minimum_size<<", maximum_size = "<<freeList.maximum_size<<", count = "<<freeList.count<<" , head = "<<freeList.head<<" ) {"<<std::endl;

        size_t freePosition = freeList.head;
        while(freePosition !=0 && freePosition < capacity)
        {
            auto& slot = memory[freePosition];
            std::cout<<"      slot "<<freePosition<<" { "<<slot.previous<<", "<<slot.next<<", "<<slot.status
                     <<" } previous = "<<memory[freePosition+1].index<<", next = "<<memory[freePosition+2].index<<std::endl;
            freePosition = memory[freePosition+2].index;
        }

        std::cout<<"   }"<<std::endl;
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
    Allocator(std::move(in_nestedAllocator))
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

    DEBUG_MESSAGE << "IntrusiveAllocator()" << this<<std::endl;
}

IntrusiveAllocator::~IntrusiveAllocator()
{
    DEBUG_MESSAGE << "~IntrusiveAllocator() " << this << std::endl;
}

void IntrusiveAllocator::setBlockSize(AllocatorAffinity allocatorAffinity, size_t blockSize)
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

void* IntrusiveAllocator::allocate(std::size_t size, AllocatorAffinity allocatorAffinity)
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
            DEBUG_MESSAGE<<"1 IntrusiveAllocator::allocate("<<size<<", "<<allocatorAffinity<<") ptr = "<<mem_ptr<<std::endl;
            return mem_ptr;
        }
    }

    DEBUG_MESSAGE<<"2 Fall through IntrusiveAllocator::allocate("<<size<<", "<<allocatorAffinity<<")"<<std::endl;

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
                DEBUG_MESSAGE<<"A Allocator::deallocate("<<ptr<<", "<<size<<") memory = "<<itr->first<<std::endl;
                return true;
            }
            else
            {
                DEBUG_MESSAGE<<"B failed Allocator::deallocate("<<ptr<<", "<<size<<") memory = "<<itr->first<<std::endl;
            }
        }
        else
        {
            auto& block = itr->second;
            if (block->deallocate(ptr, size))
            {
                DEBUG_MESSAGE<<"C Allocator::deallocate("<<ptr<<", "<<size<<") memory = "<<itr->first<<std::endl;
                return true;
            }
            else
            {
                DEBUG_MESSAGE<<"D failed Allocator::deallocate("<<ptr<<", "<<size<<") memory = "<<itr->first<<std::endl;
            }
        }
    }
    else
    {
        auto& block = memoryBlocks.rbegin()->second;
        if (block->deallocate(ptr, size))
        {
            DEBUG_MESSAGE<<"E Allocator::deallocate("<<ptr<<", "<<size<<") memoryBlocks.rbegin()->first = "<<memoryBlocks.rbegin()->first<<std::endl;
            return true;
        }
        else
        {
            DEBUG_MESSAGE<<"F failed Allocator::deallocate("<<ptr<<", "<<size<<") memoryBlocks.rbegin()->first = "<<memoryBlocks.rbegin()->first<<std::endl;
        }
    }


#endif


    if (nestedAllocator && nestedAllocator->deallocate(ptr, size))
    {
        DEBUG_MESSAGE<<"G Fall through nestedAllocator->deallocate("<<ptr<<", "<<size<<")"<<std::endl;
        return true;
    }

    DEBUG_MESSAGE<<"H Fall through"<<std::endl;\

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


size_t IntrusiveAllocator::deleteEmptyMemoryBlocks() { vsg::info("IntrusiveAllocator::deleteEmptyMemoryBlocks(..) TODO"); return 0; }
size_t IntrusiveAllocator::totalAvailableSize() const { vsg::info("IntrusiveAllocator::totalAvailableSize(..) TODO"); return 0; }
size_t IntrusiveAllocator::totalReservedSize() const { vsg::info("IntrusiveAllocator::totalReservedSize(..) TODO"); return 0; }
size_t IntrusiveAllocator::totalMemorySize() const { vsg::info("IntrusiveAllocator::totalMemorySize(..) TODO");   return 0; }
void IntrusiveAllocator::setMemoryTracking(int) { vsg::info("IntrusiveAllocator::setMemoryTracking(..) TODO"); }
