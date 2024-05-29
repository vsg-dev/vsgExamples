#pragma once

#include <array>
#include <memory>

#include <vsg/core/Allocator.h>

namespace experimental
{

extern bool& debug();

class CustomMemorySlots
{
public:

    CustomMemorySlots(size_t size);

    using OptionalOffset = std::pair<bool, size_t>;
    OptionalOffset reserve(size_t size, size_t alignment);
    bool release(size_t offset, size_t size);

    std::vector<size_t> offsets;
    std::vector<size_t> availableSlots;
};

struct CustomMemoryBlock
{
    CustomMemoryBlock(size_t in_blockSize, size_t in_alignment);
    virtual ~CustomMemoryBlock();

    void* allocate(std::size_t size);
    bool deallocate(void* ptr, std::size_t size);

    void report(std::ostream& out) const;

    CustomMemorySlots memorySlots;
    size_t alignment = 4;
    size_t block_alignment = 16;
    size_t blockSize;
    uint8_t* memory = nullptr;
};


class CustomAllocator : public vsg::Allocator
{
public:
    CustomAllocator(std::unique_ptr<Allocator> in_nestedAllocator = {});

    ~CustomAllocator();

    void report(std::ostream& out) const override;

    void* allocate(std::size_t size, vsg::AllocatorAffinity allocatorAffinity = vsg::ALLOCATOR_AFFINITY_OBJECTS) override;

    bool deallocate(void* ptr, std::size_t size) override;

    //std::vector<std::unique_ptr<CustomMemoryBlock>> allocatorMemoryBlocks;
    std::shared_ptr<CustomMemoryBlock> memoryBlock;
};

} // namespace experimental
