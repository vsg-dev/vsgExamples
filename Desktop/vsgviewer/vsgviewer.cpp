#include <vsg/all.h>

#include <iostream>
#include <chrono>
#include <thread>

#include "AnimationPath.h"


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto windowTraits = vsg::Window::Traits::create();
    windowTraits->windowTitle = "osg2vsg";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    auto numFrames = arguments.value(-1, "-f");
    auto printFrameRate = arguments.read("--fr");
    auto sleepTime = arguments.value(0.0, "--sleep");
    auto pathFilename = arguments.value(std::string(),"-p");
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    VsgNodes vsgNodes;

    // read any vsg files
    for (int i=1; i<argc; ++i)
    {
        std::string filename = arguments[i];
        auto ext = vsg::fileExtension(filename);
        if (ext=="vsga")
        {
            filename = vsg::findFile(filename, searchPaths);
            if (!filename.empty())
            {
                std::ifstream fin(filename);
                vsg::AsciiInput input(fin);

                vsg::ref_ptr<vsg::Node> loaded_scene = input.readObject<vsg::Node>("Root");
                if (loaded_scene) vsgNodes.push_back(loaded_scene);

                std::cout<<"vsg ascii file "<<filename<<" loaded_scene="<<loaded_scene.get()<<std::endl;

                arguments.remove(i, 1);
                --i;
            }
        }
        else if (ext=="vsgb")
        {
            filename = vsg::findFile(filename, searchPaths);
            if (!filename.empty())
            {
                std::ifstream fin(filename, std::ios::in | std::ios::binary);
                vsg::BinaryInput input(fin);

                std::cout<<"vsg binary file "<<filename<<std::endl;

                vsg::ref_ptr<vsg::Node> loaded_scene = input.readObject<vsg::Node>("Root");
                if (loaded_scene) vsgNodes.push_back(loaded_scene);

                arguments.remove(i, 1);
                --i;
            }
        }
    }

    // assign the vsg_scene from the loaded/converted nodes
    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (vsgNodes.size()>1)
    {
        auto vsg_group = vsg::Group::create();
        for(auto& subgraphs : vsgNodes)
        {
            vsg_group->addChild(subgraphs);
        }

        vsg_scene = vsg_group;
    }
    else if (vsgNodes.size()==1)
    {
        vsg_scene = vsgNodes.front();
    }


    if (!vsg_scene)
    {
        std::cout<<"No command graph created."<<std::endl;
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);


    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;

    // set up the camera
    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, radius * 4.5));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, vsg::ViewportState::create(window->extent2D())));


    // add a GraphicsStage tp the Window to do dispatch of the command graph to the commnad buffer(s)
    window->addStage(vsg::GraphicsStage::create(vsg_scene, camera));

    // compile the Vulkan objects
    viewer->compile();

    // add close handler to respond the close window button and pressing esape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));


    if (pathFilename.empty())
    {
        viewer->addEventHandler(vsg::Trackball::create(camera));
    }
    else
    {
        std::ifstream in(pathFilename);
        if (!in)
        {
            std::cout << "AnimationPat: Could not open animation path file \"" << pathFilename << "\".\n";
            return 1;
        }

        vsg::ref_ptr<vsg::AnimationPath> animationPath(new vsg::AnimationPath);
        //animationPath->setLoopMode(osg::AnimationPath::LOOP);
        animationPath->read(in);

        viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
    }

    double time = 0.0;
    while (viewer->active() && (numFrames<0 || (numFrames--)>0))
    {
        // poll events and advance frame counters
        viewer->advance();

        double previousTime = time;
        time = std::chrono::duration<double, std::chrono::seconds::period>(std::chrono::steady_clock::now()-viewer->start_point()).count();
        if (printFrameRate) std::cout<<"time = "<<time<<" fps="<<1.0/(time-previousTime)<<std::endl;

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        if (viewer->aquireNextFrame())
        {
            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }

        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(sleepTime));
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
