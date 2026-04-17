#include "SharedPtrNode.h"

namespace experimental
{

    void SharedPtrNode::accept(SharedPtrVisitor& spv)
    {
        spv.apply(*this);
    }
    void SharedPtrNode::traverse(SharedPtrVisitor&)
    {
    }

    void SharedPtrGroup::accept(SharedPtrVisitor& spv)
    {
        spv.apply(*this);
    }
    void SharedPtrGroup::traverse(SharedPtrVisitor& spv)
    {
        for (const auto& child : _children) child->accept(spv);
    }

    void SharedPtrQuadGroup::accept(SharedPtrVisitor& spv)
    {
        spv.apply(*this);
    }
    void SharedPtrQuadGroup::traverse(SharedPtrVisitor& spv)
    {
        for (int i = 0; i < 4; ++i) _children[i]->accept(spv);
    }

} // namespace experimental
