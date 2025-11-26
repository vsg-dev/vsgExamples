#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

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

    // if we want to redirect std::cout and std::cerr to the vsg::Logger call vsg::Logger::redirect_stdout()
    if (arguments.read({"--redirect-std", "-r"})) vsg::Logger::instance()->redirect_std();

    // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
    options->sharedObjects = vsg::SharedObjects::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    options->readOptions(arguments);

    size_t iterationCount = arguments.value<size_t>(100000, "-i");

    auto root = vsg::Group::create();

    vsg::Path path;

    // read any vsg files
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        path = vsg::filePath(filename);

        auto object = vsg::read(filename, options);
        if (auto node = object.cast<vsg::Node>())
        {
            root->addChild(node);
        }
        else if (object)
        {
            std::cout << "Unable to view object of type " << object->className() << std::endl;
        }
        else
        {
            std::cout << "Unable to load file " << filename << std::endl;
        }
    }

    if (root->children.empty())
    {
        for(size_t i = 0; i<1000; ++i)
        {
            if (i%3==0) root->addChild(vsg::Node::create());
            else if (i%3==1) root->addChild(vsg::VertexDraw::create());
            else if (i%3==2) root->addChild(vsg::Geometry::create());
        }
    }

    if (VSG_USE_dynamic_cast)
    {
        std::cout<<"Using dynamic_cast<> based RTTI vsg::Object::cast<> implementation."<<std::endl;
    }
    else
    {
        std::cout<<"Using typeid() based RTTI vsg::Object::cast<> implementation - faster than using dynamic_cast<>."<<std::endl;
    }

    auto startTime = vsg::clock::now();

    size_t count = 0;

    for(size_t i=0; i<iterationCount; ++i)
    {
        count += (1 + traverseChildren(root));
    }

    auto time = std::chrono::duration<float, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
    std::cout << "Time " << time*1000.0 << "ms" << " count = " << count << std::endl;
    std::cout << "Cast and traverse per second " << static_cast<size_t>((static_cast<double>(count)/time))<< std::endl;

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
