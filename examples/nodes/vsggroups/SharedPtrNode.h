#pragma once

#include <array>
#include <memory>
#include <vector>

#include <vsg/core/Allocator.h>

namespace experimental
{

    class SharedPtrVisitor;

    class SharedPtrAuxiliary : public std::enable_shared_from_this<SharedPtrAuxiliary>
    {
    public:
        SharedPtrAuxiliary() {}

    protected:
        virtual ~SharedPtrAuxiliary() {}
    };

    class SharedPtrNode : public std::enable_shared_from_this<SharedPtrNode>
    {
    public:
        SharedPtrNode() {}

        virtual void accept(SharedPtrVisitor& spv);
        virtual void traverse(SharedPtrVisitor& spv);

        virtual ~SharedPtrNode() {}

    protected:
        std::shared_ptr<SharedPtrAuxiliary> _auxiliary;
    };

    class SharedPtrGroup;
    class SharedPtrQuadGroup;

    class SharedPtrVisitor
    {
    public:
        virtual void apply(SharedPtrNode&) {}
        virtual void apply(SharedPtrGroup&) {}
        virtual void apply(SharedPtrQuadGroup&) {}
    };

    class SharedPtrGroup : public SharedPtrNode
    {
    public:
        SharedPtrGroup() {}
        SharedPtrGroup(size_t size) :
            _children(size, nullptr)
        {}

        virtual void accept(SharedPtrVisitor& spv);
        virtual void traverse(SharedPtrVisitor& spv);

        using Children = std::vector<std::shared_ptr<SharedPtrNode>, vsg::allocator_affinity_nodes<std::shared_ptr<SharedPtrNode>>>;

        void setChild(std::size_t i, std::shared_ptr<SharedPtrNode> node) { _children[i] = node; }
        SharedPtrNode* getChild(std::size_t i) { return _children[i].get(); }
        const SharedPtrNode* getChild(std::size_t i) const { return _children[i].get(); }

        virtual ~SharedPtrGroup() {}

    protected:
        Children _children;
    };

    class SharedPtrQuadGroup : public SharedPtrNode
    {
    public:
        SharedPtrQuadGroup() {}

        virtual void accept(SharedPtrVisitor& spv);
        virtual void traverse(SharedPtrVisitor& spv);

        using Children = std::array<std::shared_ptr<SharedPtrNode>, 4>;

        void setChild(std::size_t i, std::shared_ptr<SharedPtrNode> node) { _children[i] = node; }
        SharedPtrNode* getChild(std::size_t i) { return _children[i].get(); }
        const SharedPtrNode* getChild(std::size_t i) const { return _children[i].get(); }

        virtual ~SharedPtrQuadGroup() {}

    protected:
        Children _children;
    };

} // namespace experimental
