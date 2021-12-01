#include <vsg/core/Auxiliary.h>
#include <vsg/core/Object.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/io/Options.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <stack>
#include <typeindex>
#include <unordered_map>
#include <vector>

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    auto numObjects = arguments.value(1000000u, {"---num-objects", "-n"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    using Objects = std::vector<vsg::ref_ptr<vsg::Object>>;
    Objects objects;
    objects.reserve(numObjects);
    for (unsigned int i = 0; i < numObjects; ++i)
    {
        objects.push_back(vsg::Node::create());
    }

    Objects copy_objects(numObjects);

    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();

    copy_objects = objects;
    copy_objects.clear();

    std::cout << "Time to copy container with "<<numObjects<<" objects : " << std::chrono::duration<double>(clock::now() - start).count()<<" seconds."<<std::endl;

    return 0;
}
