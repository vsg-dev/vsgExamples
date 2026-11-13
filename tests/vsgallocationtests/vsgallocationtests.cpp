#include <vsg/all.h>
#include <vsgXchange/all.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

size_t traverseChildren(const vsg::Group* group)
{
    size_t count = group->children.size();
    for(auto& child : group->children)
    {
        if (auto child_group = child.cast<vsg::Group>()) count += traverseChildren(child_group);
    }
    return count;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    if (arguments.read("--args")) std::cout << arguments << std::endl;

    size_t iterationCount = arguments.value<size_t>(200, "-i");
    vsg::Path filename = arguments.value<vsg::Path>("textures/left_eye.jpg", "-f");

    auto root = vsg::Group::create();

    auto startTime = vsg::clock::now();

    vsg::Path path;
    for (size_t i = 0; i < iterationCount; ++i)
    {
        auto options = vsg::Options::create();
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
        options->add(vsgXchange::all::create());

        auto data = vsg::read_cast<vsg::Data>(filename, options);

        vsg::GeometryInfo geometry;
        vsg::StateInfo state;
        state.image = data;

        auto builder = vsg::Builder::create();
        auto quad = builder->createQuad(geometry, state);

        std::cout<<i<<", data = "<<data<<", quad = "<<quad<<std::endl;
    }


    auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(vsg::clock::now() - startTime).count();
    std::cout << "Time " << time << "ms" << " iterationCount = " << iterationCount << std::endl;

    return 0;
}
