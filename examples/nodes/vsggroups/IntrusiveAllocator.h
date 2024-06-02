#pragma once

#include <array>
#include <memory>

#include <vsg/core/Allocator.h>

#include "CustomAllocator.h"

namespace experimental
{

class IntrusiveAllocator;

struct IntrusiveMemoryBlock
{
    IntrusiveMemoryBlock(const std::string& in_name, size_t in_blockSize, size_t in_alignment);
    virtual ~IntrusiveMemoryBlock();

    std::string name;

    void* allocate(std::size_t size);
    bool deallocate(void* ptr, std::size_t size);

    void report(std::ostream& out) const;

    using value_type = uint32_t;
    value_type* memory = nullptr;
    value_type* memory_end = nullptr;
    value_type capacity = 0;

    size_t alignment = 4; // min aligment is 4.
    size_t block_alignment = 16;
    size_t blockSize = 0;

    value_type firstFree = 0;

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
    std::map<void*, std::shared_ptr<IntrusiveMemoryBlock>> memoryBlocks;
    std::shared_ptr<IntrusiveMemoryBlock> memoryBlockWithSpace;

    void* allocate(std::size_t size);
    bool deallocate(void* ptr, std::size_t size);
    void report(std::ostream& out) const;
};


class IntrusiveAllocator : public vsg::Allocator
{
public:
    IntrusiveAllocator(std::unique_ptr<Allocator> in_nestedAllocator = {});

    ~IntrusiveAllocator();

    void report(std::ostream& out) const override;

    void* allocate(std::size_t size, vsg::AllocatorAffinity allocatorAffinity = vsg::ALLOCATOR_AFFINITY_OBJECTS) override;

    bool deallocate(void* ptr, std::size_t size) override;

    std::vector<std::unique_ptr<IntrusiveMemoryBlocks>> callocatorMemoryBlocks;
    std::map<void*, std::shared_ptr<IntrusiveMemoryBlock>> memoryBlocks;
};

} // namespace experimental
