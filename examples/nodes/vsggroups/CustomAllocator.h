#pragma once

#include <array>
#include <memory>

#include <vsg/core/Allocator.h>

namespace experimental
{

extern bool& debug();

class CustomAllocator;

class CustomMemorySlots
{
public:

    CustomMemorySlots(size_t size);
#if 0
    using Offset = size_t;
    using Index = size_t;
#else
    using Offset = uint32_t;
    using Index = uint32_t;
#endif
    using OptionalOffset = std::pair<bool, size_t>;
    OptionalOffset reserve(size_t size, size_t alignment);
    bool release(size_t offset, size_t size);

    std::vector<Offset> offsets;
    std::vector<Index> availableSlots;
};

struct CustomMemoryBlock
{
    CustomMemoryBlock(const std::string& in_name, size_t in_blockSize, size_t in_alignment);
    virtual ~CustomMemoryBlock();

    std::string name;

    void* allocate(std::size_t size);
    bool deallocate(void* ptr, std::size_t size);

    void report(std::ostream& out) const;

    CustomMemorySlots memorySlots;
    size_t alignment = 4;
    size_t block_alignment = 16;
    size_t blockSize;
    uint8_t* memory = nullptr;
    uint8_t* memory_end = nullptr;

    inline bool within(void* ptr) const { return memory <= ptr && ptr < memory_end; }
};

class CustomMemoryBlocks
{
public:
    CustomMemoryBlocks(CustomAllocator* in_parent, const std::string& in_name, size_t in_blockSize, size_t in_alignment);
    virtual ~CustomMemoryBlocks();

    CustomAllocator* parent = nullptr;
    std::string name;
    size_t alignment = 4;
    size_t blockSize;
    std::map<void*, std::shared_ptr<CustomMemoryBlock>> memoryBlocks;
    std::shared_ptr<CustomMemoryBlock> memoryBlockWithSpace;

    void* allocate(std::size_t size);
    bool deallocate(void* ptr, std::size_t size);
    void report(std::ostream& out) const;
};


class CustomAllocator : public vsg::Allocator
{
public:
    CustomAllocator(std::unique_ptr<Allocator> in_nestedAllocator = {});

    ~CustomAllocator();

    void report(std::ostream& out) const override;

    void* allocate(std::size_t size, vsg::AllocatorAffinity allocatorAffinity = vsg::ALLOCATOR_AFFINITY_OBJECTS) override;

    bool deallocate(void* ptr, std::size_t size) override;

    std::vector<std::unique_ptr<CustomMemoryBlocks>> callocatorMemoryBlocks;
    std::map<void*, std::shared_ptr<CustomMemoryBlock>> memoryBlocks;
};

} // namespace experimental
