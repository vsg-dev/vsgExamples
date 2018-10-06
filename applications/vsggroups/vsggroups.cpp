#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/utils/CommandLine.h>

#ifdef VSG_HAS_DISPATCH_TRAVERSAL
#include <vsg/traversals/DispatchTraversal.h>
#endif

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

#define INLINE_TRAVERSE

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



osg::Node* createOsgQuadTree(unsigned int numLevels)
{
    if (numLevels==0) return new osg::Node;

    osg::Group* t = new osg::Group;

    --numLevels;

    t->addChild(createOsgQuadTree(numLevels));
    t->addChild(createOsgQuadTree(numLevels));
    t->addChild(createOsgQuadTree(numLevels));
    t->addChild(createOsgQuadTree(numLevels));

    return t;
}

vsg::Node* createVsgQuadTree(unsigned int numLevels)
{
    if (numLevels==0) return new vsg::Node;

#if 1
    vsg::Group* t = new vsg::Group(4);

    --numLevels;

    t->setChild(0, createVsgQuadTree(numLevels));
    t->setChild(1, createVsgQuadTree(numLevels));
    t->setChild(2, createVsgQuadTree(numLevels));
    t->setChild(3, createVsgQuadTree(numLevels));
#else
    vsg::Group* t = new vsg::Group;

    --numLevels;

    t->getChildren().reserve(4);

    t->addChild(createVsgQuadTree(numLevels));
    t->addChild(createVsgQuadTree(numLevels));
    t->addChild(createVsgQuadTree(numLevels));
    t->addChild(createVsgQuadTree(numLevels));
#endif

    return t;
}


vsg::Node* createFixedQuadTree(unsigned int numLevels)
{
    if (numLevels==0) return new vsg::Node;

    vsg::QuadGroup* t = new vsg::QuadGroup();

    --numLevels;

    t->setChild(0, createFixedQuadTree(numLevels));
    t->setChild(1, createFixedQuadTree(numLevels));
    t->setChild(2, createFixedQuadTree(numLevels));
    t->setChild(3, createFixedQuadTree(numLevels));

    return t;
}

std::shared_ptr<experimental::SharedPtrNode> createSharedPtrQuadTree(unsigned int numLevels)
{
    if (numLevels==0) return std::make_shared<experimental::SharedPtrNode>();

    std::shared_ptr<experimental::SharedPtrQuadGroup> t = std::make_shared<experimental::SharedPtrQuadGroup>();

    --numLevels;

    t->setChild(0, createSharedPtrQuadTree(numLevels));
    t->setChild(1, createSharedPtrQuadTree(numLevels));
    t->setChild(2, createSharedPtrQuadTree(numLevels));
    t->setChild(3, createSharedPtrQuadTree(numLevels));

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

#ifdef VSG_HAS_DISPATCH_TRAVERSAL
    vsg::ref_ptr<vsg::DispatchTraversal> vsg_dispatchTraversal;
#endif

    try
    {
        vsg::CommandLine::read(argc, argv, vsg::CommandLine::Match("--levels", "-l"), numLevels);
        vsg::CommandLine::read(argc, argv, vsg::CommandLine::Match("--traversals", "-t"), numTraversals);
        vsg::CommandLine::read(argc, argv, "--type", type);
        if (vsg::CommandLine::read(argc, argv, "-q")) { printTimingInfo = false; }
#ifdef VSG_HAS_DISPATCH_TRAVERSAL
        if (vsg::CommandLine::read(argc, argv, "-d")) { vsg_dispatchTraversal = new vsg::DispatchTraversal; }
#endif
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    clock::time_point start = clock::now();

    vsg::ref_ptr<vsg::Node> vsg_root;
    osg::ref_ptr<osg::Node> osg_root;
    std::shared_ptr<experimental::SharedPtrNode> shared_root;

    if (type=="vsg::Group") vsg_root = createVsgQuadTree(numLevels);
    if (type=="vsg::QuadGroup") vsg_root = createFixedQuadTree(numLevels);
    if (type=="osg::Group") osg_root = createOsgQuadTree(numLevels);
    if (type=="SharedPtrGroup") shared_root = createSharedPtrQuadTree(numLevels)->shared_from_this();

    if (!vsg_root && !osg_root && !shared_root)
    {
        std::cout<<"Error invalid type="<<type<<std::endl;
        return 1;
    }

    clock::time_point after_construction = clock::now();

    unsigned int numNodesVisited = 0;
    unsigned int numNodes = 0;

    if (vsg_root)
    {
#ifdef VSG_HAS_DISPATCH_TRAVERSAL
        if (vsg_dispatchTraversal)
        {
            for(unsigned int i=0; i<numTraversals; ++i)
            {
                vsg_root->accept(*vsg_dispatchTraversal);
                numNodesVisited += vsg_dispatchTraversal->numNodes;
                numNodes = vsg_dispatchTraversal->numNodes;
                vsg_dispatchTraversal->numNodes = 0;
            }
        }
        else
#endif
        {
            vsg::ref_ptr<VsgVisitor> vsg_visitor = new VsgVisitor;

            for(unsigned int i=0; i<numTraversals; ++i)
            {
                vsg_root->accept(*vsg_visitor);
                numNodesVisited += vsg_visitor->numNodes;
                numNodes = vsg_visitor->numNodes;
                vsg_visitor->numNodes = 0;
            }
        }
    }
    else if (osg_root)
    {
        for(unsigned int i=0; i<numTraversals; ++i)
        {
            OsgVisitor visitor;
            osg_root->accept(visitor);
            numNodesVisited += visitor.numNodes;
            numNodes = visitor.numNodes;
        }
    }
    else if (shared_root)
    {
        ExperimentVisitor experimentVisitor;

        for(unsigned int i=0; i<numTraversals; ++i)
        {
            shared_root->accept(experimentVisitor);
            numNodesVisited += experimentVisitor.numNodes;
            numNodes = experimentVisitor.numNodes;
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
