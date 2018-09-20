#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/nodes/LOD.h>
#include <vsg/utils/CommandLine.h>

#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <stack>
#include <typeindex>


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
    unsigned int numObjects = 1000000;
    try
    {
        vsg::CommandLine::read(argc, argv, vsg::CommandLine::Match("---num-objects", "-n"), numObjects);
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    using Objects = std::vector<vsg::ref_ptr<vsg::Object>>;
    Objects objects;
    objects.reserve(numObjects);
    for(unsigned int i=0; i<numObjects; ++i)
    {
        objects.push_back(new vsg::Node);
    }

    Objects copy_objects(numObjects);

    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();

    copy_objects = objects;
    copy_objects.clear();

    std::cout<<"Time to copy container "<<std::chrono::duration<double>(clock::now()-start).count();

    return 0;
}
