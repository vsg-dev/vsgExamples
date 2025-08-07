#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>


vsg::ref_ptr<vsg::Node> createScene(vsg::CommandLine& arguments, vsg::ref_ptr<vsg::Options> options)
{
    auto group = vsg::Group::create();

    vsg::Path path;

    // read any vsg files
    for (int i = 1; i < arguments.argc(); ++i)
    {
        vsg::Path filename = arguments[i];
        path = vsg::filePath(filename);

        auto object = vsg::read(filename, options);
        if (auto node = object.cast<vsg::Node>())
        {
            group->addChild(node);
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

    if (group->children.empty())
        return {};
    else if (group->children.size() == 1)
        return group->children[0];
    else
        return group;
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // create windowTraits using the any command line arugments to configure settings
        auto windowTraits = vsg::WindowTraits::create(arguments);

        // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

        bool reportAverageFrameRate = arguments.read("--fps");
        if (arguments.read({"-t", "--test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->fullscreen = true;
            reportAverageFrameRate = true;
        }
        if (arguments.read({"--st", "--small-test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->width = 192, windowTraits->height = 108;
            windowTraits->decoration = false;
            reportAverageFrameRate = true;
        }

        auto numFrames = arguments.value(-1, "-f");
        auto nearFarRatio = arguments.value<double>(0.001, "--nfr");

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        auto vsg_scene = createScene(arguments, options);

        if (!vsg_scene)
        {
            std::cout << "Please specify a 3d model on the command line." << std::endl;
            return 1;
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);

        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.5);
        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        viewer->start_point() = vsg::clock::now();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }

        if (reportAverageFrameRate)
        {
            auto fs = viewer->getFrameStamp();
            double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
            std::cout << "Average frame rate = " << fps << " fps" << std::endl;
        }
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
