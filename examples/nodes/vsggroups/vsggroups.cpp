#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

#include "SharedPtrNode.h"

//#define INLINE_TRAVERSE

class VsgVisitor : public vsg::Visitor
{
public:
    uint64_t numNodes = 0;

    using Visitor::apply;

    void apply(vsg::Object& object) final
    {
        //std::cout<<"VsgVisitor::apply(vsg::Object&) "<<typeid(object).name()<<std::endl;
        ++numNodes;
        object.traverse(*this);
    }

    void apply(vsg::Group& group) final
    {
        //std::cout<<"VsgVisitor::apply(vsg::Group&)"<<std::endl;
        ++numNodes;
#ifdef INLINE_TRAVERSE
        vsg::Group::t_traverse(group, *this);
#else
        group.traverse(*this);
#endif
    }

    void apply(vsg::QuadGroup& group) final
    {
        //std::cout<<"VsgVisitor::apply(vsg::QuadGroup&)"<<std::endl;
        ++numNodes;
#ifdef INLINE_TRAVERSE
        vsg::QuadGroup::t_traverse(group, *this);
#else
        group.traverse(*this);
#endif
    }
};

class VsgConstVisitor : public vsg::ConstVisitor
{
public:
    uint64_t numNodes = 0;

    using ConstVisitor::apply;

    void apply(const vsg::Object& object) final
    {
        //std::cout<<"VsgVisitor::apply(vsg::Object&) "<<typeid(object).name()<<std::endl;
        ++numNodes;
        object.traverse(*this);
    }

    void apply(const vsg::Group& group) final
    {
        //std::cout<<"VsgVisitor::apply(vsg::Group&)"<<std::endl;
        ++numNodes;
#ifdef INLINE_TRAVERSE
        vsg::Group::t_traverse(group, *this);
#else
        group.traverse(*this);
#endif
    }

    void apply(const vsg::QuadGroup& group) final
    {
        //std::cout<<"VsgVisitor::apply(vsg::QuadGroup&)"<<std::endl;
        ++numNodes;
#ifdef INLINE_TRAVERSE
        vsg::QuadGroup::t_traverse(group, *this);
#else
        group.traverse(*this);
#endif
    }
};

class ExperimentVisitor : public experimental::SharedPtrVisitor
{
public:
    uint64_t numNodes = 0;

    using SharedPtrVisitor::apply;

    void apply(experimental::SharedPtrNode& object) final
    {
        //std::cout<<"ExperimentVisitor::apply(vsg::Object&) "<<typeid(object).name()<<std::endl;
        ++numNodes;
        object.traverse(*this);
    }

    void apply(experimental::SharedPtrQuadGroup& group) final
    {
        //std::cout<<"ExperimentVisitor::apply(vsg::SharedPtrQuadGroup&)"<<std::endl;
        ++numNodes;
        group.traverse(*this);
    }
};

vsg::ref_ptr<vsg::Node> createVsgQuadTree(uint64_t numLevels, uint64_t& numNodes, uint64_t& numBytes)
{
    if (numLevels == 0)
    {
        numNodes += 1;
        numBytes += sizeof(vsg::Node);

        return vsg::Node::create();
    }

    auto t = vsg::Group::create(4);

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(vsg::Group) + 4 * sizeof(vsg::ref_ptr<vsg::Node>);

    t->children[0] = createVsgQuadTree(numLevels, numNodes, numBytes);
    t->children[1] = createVsgQuadTree(numLevels, numNodes, numBytes);
    t->children[2] = createVsgQuadTree(numLevels, numNodes, numBytes);
    t->children[3] = createVsgQuadTree(numLevels, numNodes, numBytes);

    return t;
}

vsg::ref_ptr<vsg::Node> createFixedQuadTree(uint64_t numLevels, uint64_t& numNodes, uint64_t& numBytes)
{
    if (numLevels == 0)
    {
        numNodes += 1;
        numBytes += sizeof(vsg::Node);

        return vsg::Node::create();
    }

    auto t = vsg::QuadGroup::create();

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(vsg::QuadGroup);

    t->children = {{createFixedQuadTree(numLevels, numNodes, numBytes),
                    createFixedQuadTree(numLevels, numNodes, numBytes),
                    createFixedQuadTree(numLevels, numNodes, numBytes),
                    createFixedQuadTree(numLevels, numNodes, numBytes)}};

    return t;
}

std::shared_ptr<experimental::SharedPtrNode> createSharedPtrQuadTree(uint64_t numLevels, uint64_t& numNodes, uint64_t& numBytes)
{
    if (numLevels == 0)
    {
        numNodes += 1;
        numBytes += sizeof(experimental::SharedPtrNode);

        return std::make_shared<experimental::SharedPtrNode>();
    }

    std::shared_ptr<experimental::SharedPtrQuadGroup> t = std::make_shared<experimental::SharedPtrQuadGroup>();

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(experimental::SharedPtrQuadGroup);

    t->setChild(0, createSharedPtrQuadTree(numLevels, numNodes, numBytes));
    t->setChild(1, createSharedPtrQuadTree(numLevels, numNodes, numBytes));
    t->setChild(2, createSharedPtrQuadTree(numLevels, numNodes, numBytes));
    t->setChild(3, createSharedPtrQuadTree(numLevels, numNodes, numBytes));

    return t;
}

// consider tcmalloc? https://goog-perftools.sourceforge.net/doc/tcmalloc.html
// consider Alloc https://www.codeproject.com/Articles/1084801/Replace-malloc-free-with-a-Fast-Fixed-Block-Memory
class StdAllocator : public vsg::Allocator
{
public:
    StdAllocator(std::unique_ptr<Allocator> in_nestedAllocator = {}) :
        vsg::Allocator(std::move(in_nestedAllocator))
    {
    }

    ~StdAllocator()
    {
    }

    void report(std::ostream& out) const override
    {
        out << "StdAllocator::report() " << std::endl;
    }

    void* allocate(std::size_t size, vsg::AllocatorAffinity) override
    {
        return operator new(size); //, std::align_val_t{default_alignment});
    }

    bool deallocate(void* ptr, std::size_t size) override
    {
        if (nestedAllocator && nestedAllocator->deallocate(ptr, size)) return true;

        operator delete(ptr); //, std::align_val_t{default_alignment});
        return true;
    }

    size_t deleteEmptyMemoryBlocks() override { return 0; }
    size_t totalAvailableSize() const override { return 0; }
    size_t totalReservedSize() const override { return 0; }
    size_t totalMemorySize() const override { return 0; }
    void setBlockSize(vsg::AllocatorAffinity, size_t) {}
};

const size_t KB = 1024;
const size_t MB = 1024 * KB;
const size_t GB = 1024 * MB;

struct Units
{
    Units(size_t v) :
        value(v) {}

    size_t value;
};

std::ostream& operator<<(std::ostream& out, const Units& size)
{
    if (size.value > GB)
        out << static_cast<double>(size.value) / static_cast<double>(GB) << " gigabytes";
    else if (size.value > MB)
        out << static_cast<double>(size.value) / static_cast<double>(MB) << " megabytes";
    else if (size.value > KB)
        out << static_cast<double>(size.value) / static_cast<double>(KB) << " kilobytes";
    else
        out << size.value << " bytes";
    return out;
}
int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    if (arguments.read("--std")) vsg::Allocator::instance().reset(new StdAllocator(std::move(vsg::Allocator::instance())));

    auto numLevels = arguments.value(11u, {"-l", "--levels"});
    auto numTraversals = arguments.value(10u, {"-t", "--traversals"});
    auto type = arguments.value(std::string("vsg::Group"), "--type");
    auto quiet = arguments.read("-q");
    auto inputFilename = arguments.value<vsg::Path>("", "-i");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    size_t unit = arguments.value<size_t>(MB, "--unit");
    if (int allocatorType; arguments.read("--allocator", allocatorType)) vsg::Allocator::instance()->allocatorType = vsg::AllocatorType(allocatorType);
    if (size_t objectsBlockSize; arguments.read("--objects", objectsBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_OBJECTS, objectsBlockSize * unit);
    if (size_t nodesBlockSize; arguments.read("--nodes", nodesBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_NODES, nodesBlockSize * unit);
    if (size_t dataBlockSize; arguments.read("--data", dataBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_DATA, dataBlockSize * unit);

    vsg::ref_ptr<vsg::RecordTraversal> vsg_recordTraversal(arguments.read("-d") ? new vsg::RecordTraversal : nullptr);
    vsg::ref_ptr<VsgConstVisitor> vsg_ConstVisitor(arguments.read("-c") ? new VsgConstVisitor : nullptr);
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();

    vsg::ref_ptr<vsg::Node> vsg_root;
    std::shared_ptr<experimental::SharedPtrNode> shared_root;

    uint64_t numNodes = 0;
    uint64_t numBytes = 0;

    if (inputFilename)
    {
        vsg_root = vsg::read_cast<vsg::Node>(inputFilename);

        if (!vsg_root)
        {
            std::cout << "Warning: file not loaded : " << inputFilename << std::endl;
            return 1;
        }
    }
    else
    {
        if (type == "vsg::Group") vsg_root = createVsgQuadTree(numLevels, numNodes, numBytes);
        if (type == "vsg::QuadGroup") vsg_root = createFixedQuadTree(numLevels, numNodes, numBytes);
        if (type == "SharedPtrGroup") shared_root = createSharedPtrQuadTree(numLevels, numNodes, numBytes)->shared_from_this();
    }

    if (!vsg_root && !shared_root)
    {
        std::cout << "Error invalid type=" << type << std::endl;
        return 1;
    }

    clock::time_point after_construction = clock::now();

    uint64_t numNodesVisited = 0;

    if (vsg_root)
    {
        if (vsg_recordTraversal)
        {
            std::cout << "using RecordTraversal" << std::endl;
            for (uint64_t i = 0; i < numTraversals; ++i)
            {
                vsg_root->accept(*vsg_recordTraversal);
                //numNodesVisited += vsg_recordTraversal->numNodes;
                //numNodes = vsg_recordTraversal->numNodes;
                //vsg_recordTraversal->numNodes = 0;
            }
        }
        else if (vsg_ConstVisitor)
        {
            std::cout << "using VsgConstVisitor" << std::endl;
            for (uint64_t i = 0; i < numTraversals; ++i)
            {
                vsg_root->accept(*vsg_ConstVisitor);
                numNodesVisited += vsg_ConstVisitor->numNodes;
                vsg_ConstVisitor->numNodes = 0;
            }
        }
        else
        {
            vsg::ref_ptr<VsgVisitor> vsg_visitor(new VsgVisitor);
            std::cout << "using VsgVisitor" << std::endl;
            for (uint64_t i = 0; i < numTraversals; ++i)
            {
                vsg_root->accept(*vsg_visitor);
                numNodesVisited += vsg_visitor->numNodes;
                vsg_visitor->numNodes = 0;
            }
        }
    }
    else if (shared_root)
    {
        ExperimentVisitor experimentVisitor;

        for (uint64_t i = 0; i < numTraversals; ++i)
        {
            shared_root->accept(experimentVisitor);
            numNodesVisited += experimentVisitor.numNodes;
            experimentVisitor.numNodes = 0;
        }
    }

    clock::time_point after_traversal = clock::now();

    if (outputFilename)
    {
        vsg::write(vsg_root, outputFilename);
    }

    clock::time_point after_write = clock::now();

    vsg_root = 0;
    shared_root = 0;

    clock::time_point after_destruction = clock::now();

    if (!quiet)
    {
        std::cout << "type : " << type << std::endl;
        std::cout << "numNodes : " << numNodes << std::endl;
        std::cout << "numBytes : " << numBytes << std::endl;
        std::cout << "average node size : " << double(numBytes) / double(numNodes) << std::endl;
        std::cout << "numNodesVisited : " << numNodesVisited << std::endl;

        if (!inputFilename.empty())
            std::cout << "read time : " << std::chrono::duration<double>(after_construction - start).count() << std::endl;
        else
            std::cout << "construction time : " << std::chrono::duration<double>(after_construction - start).count() << std::endl;
        std::cout << "traversal time : " << std::chrono::duration<double>(after_traversal - after_construction).count() << std::endl;

        if (!outputFilename.empty()) std::cout << "write time : " << std::chrono::duration<double>(after_write - after_traversal).count() << std::endl;
        std::cout << "destruction time : " << std::chrono::duration<double>(after_destruction - after_write).count() << std::endl;

        std::cout << "total time : " << std::chrono::duration<double>(after_destruction - start).count() << std::endl;
        std::cout << std::endl;
        std::cout << "Nodes constructed per second : " << double(numNodes) / std::chrono::duration<double>(after_construction - start).count() << std::endl;
        std::cout << "Nodes visited per second     : " << double(numNodesVisited) / std::chrono::duration<double>(after_traversal - after_construction).count() << std::endl;
        std::cout << "Nodes destructed per second : " << double(numNodes) / std::chrono::duration<double>(after_destruction - after_traversal).count() << std::endl;
    }

    return 0;
}
