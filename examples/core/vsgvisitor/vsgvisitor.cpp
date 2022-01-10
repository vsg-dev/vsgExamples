#include <vsg/core/Auxiliary.h>
#include <vsg/core/Object.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>

#include <vsg/nodes/Group.h>

#include <vsg/utils/CommandLine.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <vector>

vsg::ref_ptr<vsg::Node> createQuadTree(unsigned int numLevels)
{
    if (numLevels == 0) return vsg::Node::create();

    vsg::ref_ptr<vsg::Group> t = vsg::Group::create();

    --numLevels;

    t->children.reserve(4);

    t->addChild(createQuadTree(numLevels));
    t->addChild(createQuadTree(numLevels));
    t->addChild(createQuadTree(numLevels));
    t->addChild(createQuadTree(numLevels));

    return t;
}

template<typename F>
double time(F function)
{
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();

    // do test code
    function();

    return std::chrono::duration<double>(clock::now() - start).count();
}


int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    auto numLevels = arguments.value(11u, {"--levels", "-l"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    vsg::ref_ptr<vsg::Node> scene;
    std::cout << "VulkanSceneGraph Fixed Quad Tree creation : " << time([&]() { scene = createQuadTree(numLevels); }) << std::endl;

    struct MyVisitor : public vsg::ConstVisitor
    {
        std::map<const char*, uint32_t> objectCounts;

        void apply(const vsg::Object& object) override
        {
            ++objectCounts[object.className()];

            object.traverse(*this);
        }
    };

    // explictly create vistor and call accept on the scene
    MyVisitor myVisitor;
    scene->accept(myVisitor);
    std::cout << "MyVisitor() object types s=" << myVisitor.objectCounts.size()<< std::endl;
    for(const auto [className, count] : myVisitor.objectCounts)
    {
        std::cout<<"    "<<className<<" : "<<count<<std::endl;
    }

    // same functionality but using the vsg::vist<> template conviniencefunction to construct the visitor, call accept and then return the visitor.
    auto objectCounts = vsg::visit<MyVisitor>(scene).objectCounts;
    std::cout << "\nUsing vsg::visit<>() object types s=" << myVisitor.objectCounts.size()<< std::endl;
    for(const auto [className, count] : objectCounts)
    {
        std::cout<<"    "<<className<<" : "<<count<<std::endl;
    }

    return 0;
}
