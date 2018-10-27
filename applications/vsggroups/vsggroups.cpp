#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/utils/CommandLine.h>

#include <osg/ref_ptr>
#include <osg/Referenced>
#include <osg/Group>
#include <osg/Timer>
#include <osg/Observer>
#include <osg/UserDataContainer>

#include <iostream>
#include <vector>
#include <chrono>
#include <memory>

#include "SharedPtrNode.h"

//#define INLINE_TRAVERSE

class VsgVisitor : public vsg::Visitor
{
public:

    unsigned int numNodes = 0;

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

    unsigned int numNodes = 0;

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

class OsgVisitor: public osg::NodeVisitor
{
public:

    OsgVisitor():
        osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
        numNodes(0)
    {}

    unsigned int numNodes;

    using NodeVisitor::apply;

    void apply(osg::Node& node) override
    {
        ++numNodes;
        traverse(node);
    }

};

class ExperimentVisitor : public experimental::SharedPtrVisitor
{
public:

    unsigned int numNodes = 0;

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



osg::Node* createOsgQuadTree(unsigned int numLevels, unsigned int& numNodes, unsigned int& numBytes)
{
    if (numLevels==0)
    {
        numNodes += 1;
        numBytes += sizeof(osg::Node);

        return new osg::Node;
    }

    osg::Group* t = new osg::Group;

    numNodes += 1;
    numBytes += sizeof(osg::Group) + 4*sizeof(vsg::ref_ptr<osg::Node>);

    --numLevels;

    t->addChild(createOsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createOsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createOsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createOsgQuadTree(numLevels, numNodes, numBytes));

    return t;
}

vsg::Node* createVsgQuadTree(unsigned int numLevels, unsigned int& numNodes, unsigned int& numBytes)
{
    if (numLevels==0)
    {
        numNodes += 1;
        numBytes += sizeof(vsg::Node);

        return new vsg::Node;
    }

#if 1
    vsg::Group* t = new vsg::Group(4);

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(vsg::Group) + 4*sizeof(vsg::ref_ptr<vsg::Node>);

    t->setChild(0, createVsgQuadTree(numLevels, numNodes, numBytes));
    t->setChild(1, createVsgQuadTree(numLevels, numNodes, numBytes));
    t->setChild(2, createVsgQuadTree(numLevels, numNodes, numBytes));
    t->setChild(3, createVsgQuadTree(numLevels, numNodes, numBytes));
#else
    vsg::Group* t = new vsg::Group;

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(vsg::Group);

    t->getChildren().reserve(4);

    t->addChild(createVsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createVsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createVsgQuadTree(numLevels, numNodes, numBytes));
    t->addChild(createVsgQuadTree(numLevels, numNodes, numBytes));
#endif

    return t;
}


vsg::Node* createFixedQuadTree(unsigned int numLevels, unsigned int& numNodes, unsigned int& numBytes)
{
    if (numLevels==0)
    {
        numNodes += 1;
        numBytes += sizeof(vsg::Node);

        return new vsg::Node;
    }

    vsg::QuadGroup* t = new vsg::QuadGroup();

    --numLevels;

    numNodes += 1;
    numBytes += sizeof(vsg::QuadGroup);

    t->setChild(0, createFixedQuadTree(numLevels, numNodes, numBytes));
    t->setChild(1, createFixedQuadTree(numLevels, numNodes, numBytes));
    t->setChild(2, createFixedQuadTree(numLevels, numNodes, numBytes));
    t->setChild(3, createFixedQuadTree(numLevels, numNodes, numBytes));

    return t;
}

std::shared_ptr<experimental::SharedPtrNode> createSharedPtrQuadTree(unsigned int numLevels, unsigned int& numNodes, unsigned int& numBytes)
{
    if (numLevels==0)
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

class ElapsedTime
{
public:
    using clock = std::chrono::high_resolution_clock;
    clock::time_point _start;

    ElapsedTime()
    {
        start();
    }

    void start()
    {
        _start = clock::now();
    }

    double duration() const
    {
        return std::chrono::duration<double>(clock::now()-_start).count();
    }
};

template<typename F>
double time(F function)
{
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();

    // do test code
    function();

    return std::chrono::duration<double>(clock::now()-start).count();
}


int main(int argc, char** argv)
{

    using clock = std::chrono::high_resolution_clock;

    std::string type("vsg::Group");
    unsigned int numLevels = 11;
    unsigned int numTraversals = 10;
    bool printTimingInfo = true;

    vsg::ref_ptr<vsg::DispatchTraversal> vsg_dispatchTraversal;
    vsg::ref_ptr<VsgConstVisitor> vsg_ConstVisitor;

    vsg::CommandLine arguments(&argc, argv);
    arguments.read({"-l", "--levels"}, numLevels);
    arguments.read({"-t", "--traversals"}, numTraversals);
    arguments.read("--type", type);
    if (arguments.read("-q")) { printTimingInfo = false; }
    if (arguments.read("-d")) { vsg_dispatchTraversal = new vsg::DispatchTraversal; }
    if (arguments.read("-c")) { vsg_ConstVisitor = new VsgConstVisitor; }
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    clock::time_point start = clock::now();

    vsg::ref_ptr<vsg::Node> vsg_root;
    osg::ref_ptr<osg::Node> osg_root;
    std::shared_ptr<experimental::SharedPtrNode> shared_root;

    unsigned int numNodes = 0;
    unsigned int numBytes = 0;

    if (type=="vsg::Group") vsg_root = createVsgQuadTree(numLevels, numNodes, numBytes);
    if (type=="vsg::QuadGroup") vsg_root = createFixedQuadTree(numLevels, numNodes, numBytes);
    if (type=="osg::Group") osg_root = createOsgQuadTree(numLevels, numNodes, numBytes);
    if (type=="SharedPtrGroup") shared_root = createSharedPtrQuadTree(numLevels, numNodes, numBytes)->shared_from_this();

    if (!vsg_root && !osg_root && !shared_root)
    {
        std::cout<<"Error invalid type="<<type<<std::endl;
        return 1;
    }

    clock::time_point after_construction = clock::now();

    unsigned int numNodesVisited = 0;

    if (vsg_root)
    {
        if (vsg_dispatchTraversal)
        {
            std::cout<<"using DispatchTraversal"<<std::endl;
            for(unsigned int i=0; i<numTraversals; ++i)
            {
                vsg_root->accept(*vsg_dispatchTraversal);
                //numNodesVisited += vsg_dispatchTraversal->numNodes;
                //numNodes = vsg_dispatchTraversal->numNodes;
                //vsg_dispatchTraversal->numNodes = 0;
            }
        }
        else if (vsg_ConstVisitor)
        {
            std::cout<<"using VsgConstVisitor"<<std::endl;
            for(unsigned int i=0; i<numTraversals; ++i)
            {
                vsg_root->accept(*vsg_ConstVisitor);
                numNodesVisited += vsg_ConstVisitor->numNodes;
                vsg_ConstVisitor->numNodes = 0;
            }
        }
        else
        {
            vsg::ref_ptr<VsgVisitor> vsg_visitor(new VsgVisitor);
            std::cout<<"using VsgVisitor"<<std::endl;
            for(unsigned int i=0; i<numTraversals; ++i)
            {
                vsg_root->accept(*vsg_visitor);
                numNodesVisited += vsg_visitor->numNodes;
                vsg_visitor->numNodes = 0;
            }
        }
    }
    else if (osg_root)
    {
        std::cout<<"using OsgVisitor"<<std::endl;
        for(unsigned int i=0; i<numTraversals; ++i)
        {
            OsgVisitor visitor;
            osg_root->accept(visitor);
            numNodesVisited += visitor.numNodes;
        }
    }
    else if (shared_root)
    {
        ExperimentVisitor experimentVisitor;

        for(unsigned int i=0; i<numTraversals; ++i)
        {
            shared_root->accept(experimentVisitor);
            numNodesVisited += experimentVisitor.numNodes;
            experimentVisitor.numNodes = 0;
        }
    }

    clock::time_point after_traversal = clock::now();

    vsg_root = 0;
    osg_root = 0;
    shared_root = 0;

    clock::time_point after_destruction = clock::now();

    if (printTimingInfo)
    {
        std::cout<<"type : "<<type<<std::endl;
        std::cout<<"numNodes : "<<numNodes<<std::endl;
        std::cout<<"numBytes : "<<numBytes<<std::endl;
        std::cout<<"average node size : "<<double(numBytes)/double(numNodes)<<std::endl;
        std::cout<<"numNodesVisited : "<<numNodesVisited<<std::endl;
        std::cout<<"construcion time : "<<std::chrono::duration<double>(after_construction-start).count()<<std::endl;
        std::cout<<"traversal time : "<<std::chrono::duration<double>(after_traversal-after_construction).count()<<std::endl;
        std::cout<<"destrucion time : "<<std::chrono::duration<double>(after_destruction-after_traversal).count()<<std::endl;
        std::cout<<"total time : "<<std::chrono::duration<double>(after_destruction-start).count()<<std::endl;
        std::cout<<std::endl;
        std::cout<<"Nodes constructed per second : "<<double(numNodes)/std::chrono::duration<double>(after_construction-start).count()<<std::endl;
        std::cout<<"Nodes visited per second     : "<<double(numNodesVisited)/std::chrono::duration<double>(after_traversal-after_construction).count()<<std::endl;
        std::cout<<"Nodes destrctored per second : "<<double(numNodes)/std::chrono::duration<double>(after_destruction-after_traversal).count()<<std::endl;
    }

    return 0;
}
