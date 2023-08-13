#include <vsg/core/Auxiliary.h>
#include <vsg/core/Object.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/QuadGroup.h>

#include <vsg/maths/vec3.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

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

protected:
    virtual ~SharedPtrNode() {}

    std::shared_ptr<SharedPtrAuxiliary> _auxilary;
};

class SharedPtrQuadGroup : public SharedPtrNode
{
public:
    SharedPtrQuadGroup() {}

    using Children = std::array<std::shared_ptr<SharedPtrNode>, 4>;

    void setChild(std::size_t i, SharedPtrNode* node) { _children[i] = node ? node->shared_from_this() : nullptr; }
    SharedPtrNode* getChild(std::size_t i) { return _children[i].get(); }
    const SharedPtrNode* getChild(std::size_t i) const { return _children[i].get(); }

    //protected:
    virtual ~SharedPtrQuadGroup() {}

    Children _children;
};

int main(int /*argc*/, char** /*argv*/)
{

    vsg::ref_ptr<vsg::QuadGroup> ref_node = vsg::QuadGroup::create();
    std::cout << "sizeof(Object)=" << sizeof(vsg::Object) << std::endl;
    std::cout << "sizeof(atomic_uint)=" << sizeof(std::atomic_uint) << std::endl;
    std::cout << "sizeof(atomic_ushort)=" << sizeof(std::atomic_ushort) << std::endl;
    std::cout << "sizeof(Auxiliary)=" << sizeof(vsg::Auxiliary) << std::endl;
    std::cout << "sizeof(vsg::ref_ptr)=" << sizeof(ref_node) << ", sizeof(QuadGroup)=" << sizeof(vsg::QuadGroup) << std::endl;

#if 1
    std::shared_ptr<SharedPtrQuadGroup> shared_node(new SharedPtrQuadGroup);
#else
    // does not compile : error: conversion from ‘std::shared_ptr<SharedPtrNode>’ to non-scalar type ‘std::shared_ptr<SharedPtrQuadGroup>’
    std::shared_ptr<SharedPtrQuadGroup> shared_node = (new SharedPtrQuadGroup)->shared_from_this();
    requested
#endif

    std::cout << "sizeof(std::shared_ptr)=" << sizeof(shared_node) << ", sizeof(SharedPtrQuadGroup)=" << sizeof(SharedPtrQuadGroup) << std::endl;

    std::cout << std::endl
              << "Alignment int " << alignof(int) << std::endl;
    std::cout << "Alignment std::atomic_uint  " << alignof(std::atomic_uint) << std::endl;
    std::cout << "Alignment std::atomic_ushort  " << alignof(std::atomic_ushort) << std::endl;
    std::cout << "Alignment Object  " << alignof(vsg::Object) << std::endl;
    std::cout << "Alignment Object* " << alignof(vsg::Object*) << std::endl;
    std::cout << "Alignment Auxiliary  " << alignof(vsg::Auxiliary) << std::endl;
    std::cout << "Alignment Auxiliary* " << alignof(vsg::Auxiliary*) << std::endl;
    std::cout << "Alignment SharedPtrNode  " << alignof(SharedPtrNode) << std::endl;
    std::cout << "Alignment SharedPtrNode* " << alignof(SharedPtrNode*) << std::endl;

    std::cout << std::endl;
    std::cout << "std::is_standard_layout<vsg::Object> " << std::is_standard_layout<vsg::Object>::value << std::endl;
    std::cout << "std::is_standard_layout<vsg::vec3> " << std::is_standard_layout<vsg::vec3>::value << std::endl;
    std::cout << "offsetof<vsg::vec3,x> " << offsetof(vsg::vec3, x) << std::endl;
    std::cout << "offsetof<vsg::vec3,y> " << offsetof(vsg::vec3, y) << std::endl;
    std::cout << "offsetof<vsg::vec3,z> " << offsetof(vsg::vec3, z) << std::endl;

    return 0;
}
