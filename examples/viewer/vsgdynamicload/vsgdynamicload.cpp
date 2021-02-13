#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#    include <vsgXchange/ReaderWriter_all.h>
#endif

#ifdef USE_VSGGIS
#    include <vsgGIS/ReaderWriter_GDAL.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "../../shared/AnimationPath.h"


int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO realted options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    #ifdef USE_VSGXCHANGE
        options->add(vsgXchange::ReaderWriter_all::create()); // add the optional ReaderWriter_all fron vsgXchange to read 3d models and imagery
    #endif

        arguments.read(options);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgviewer";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        arguments.read("--display", windowTraits->display);
        auto numFrames = arguments.value(-1, "-f");

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc<=1)
        {
            std::cout << "Please specify a 3d model or image file on the command line." << std::endl;
            return 1;
        }

        auto vsg_scene = vsg::read_cast<vsg::Node>(arguments[1], options);
        if (!vsg_scene)
        {
            std::cout << "Could not load model." << std::endl;
            return 1;
        }


        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }
    }
    catch (const vsg::Exception& ve)
    {
        for(int i=0; i<argc; ++i) std::cerr<<argv[i]<<" ";
        std::cerr << "\n[Exception] - " << ve.message << " result = "<<ve.result<<std::endl;
        return 1;
    }


    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
