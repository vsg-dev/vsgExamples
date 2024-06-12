#pragma once

#include <array>
#include <memory>

#include <vsg/core/Allocator.h>

namespace vsg
{

extern bool& debug();

class IntrusiveAllocator;

struct IntrusiveMemoryBlock
{
    IntrusiveMemoryBlock(const std::string& in_name, size_t in_blockSize, size_t in_alignment);
    virtual ~IntrusiveMemoryBlock();

    std::string name;

    void* allocate(std::size_t size);
    bool deallocate(void* ptr, std::size_t size);

    void report(std::ostream& out) const;

    // bitfield packing of doubly-linked with status field into a 4 byte word
    struct Element
    {

        Element(size_t in_index) :
            index(in_index) {}

        Element(size_t in_previous, size_t in_next, unsigned int in_status) :
            previous(in_previous),
            next(in_next),
            status(in_status) {}

        union
        {
            uint32_t index;

            struct
            {
                unsigned int previous : 15;
                unsigned int next : 15;
                unsigned int status : 2;
            };
        };
    };

    struct FreeList
    {
        size_t minimum_size = 0;
        size_t maximum_size = 0;
        size_t count = 0;
        size_t head = 0;
    };

    using Offset = uint16_t;
    using value_type = Element;
    value_type* memory = nullptr;
    value_type* memory_end = nullptr;
    size_t capacity = 0;

    size_t alignment = 4; // min aligment is 4.
    size_t block_alignment = 16;
    size_t blockSize = 0;

    std::vector<FreeList> freeLists;

    bool validate() const;

    bool freeSlotsAvaible(size_t size) const;

    inline bool within(void* ptr) const { return memory <= ptr && ptr < memory_end; }

};

class IntrusiveMemoryBlocks
{
public:
    IntrusiveMemoryBlocks(IntrusiveAllocator* in_parent, const std::string& in_name, size_t in_blockSize, size_t in_alignment);
    virtual ~IntrusiveMemoryBlocks();

    IntrusiveAllocator* parent = nullptr;
    std::string name;
    size_t alignment = 4;
    size_t blockSize;
    std::vector<std::shared_ptr<IntrusiveMemoryBlock>> memoryBlocks;
    std::shared_ptr<IntrusiveMemoryBlock> memoryBlockWithSpace;

    void* allocate(std::size_t size);
    void report(std::ostream& out) const;
    bool validate() const;
};


class VSG_DECLSPEC IntrusiveAllocator : public Allocator
{
public:
    IntrusiveAllocator(std::unique_ptr<Allocator> in_nestedAllocator = {});

    ~IntrusiveAllocator();

    void report(std::ostream& out) const override;

    void* allocate(std::size_t size, AllocatorAffinity allocatorAffinity = vsg::ALLOCATOR_AFFINITY_OBJECTS) override;

    bool deallocate(void* ptr, std::size_t size) override;

    bool validate() const;

    size_t deleteEmptyMemoryBlocks() override;
    size_t totalAvailableSize() const override;
    size_t totalReservedSize() const override;
    size_t totalMemorySize() const override;
    void setMemoryTracking(int mt) override;
    void setBlockSize(AllocatorAffinity allocatorAffinity, size_t blockSize) override;

protected:

    friend IntrusiveMemoryBlocks;

    std::vector<std::unique_ptr<IntrusiveMemoryBlocks>> allocatorMemoryBlocks;
    std::map<void*, std::shared_ptr<IntrusiveMemoryBlock>> memoryBlocks;
};

}
