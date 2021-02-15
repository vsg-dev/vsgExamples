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

// 1. Load data
// 2. compile data.
// 3. submit compile data.
// 4. wait for submission to complete
// 5. add to main scene graph

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

        // provide setting of the resource hints on the command line
        vsg::ref_ptr<vsg::ResourceHints> resourceHints;
        if (vsg::Path resourceFile; arguments.read("--resource", resourceFile)) resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceFile);

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify a 3d models on the command line." << std::endl;
            return 1;
        }

        std::vector<std::pair<vsg::Path, vsg::ref_ptr<vsg::MatrixTransform>>> filenameAttachments;

        vsg::dvec3 origin(0.0, 0.0, 0.0);
        vsg::dvec3 primary(2.0, 0.0, 0.0);
        vsg::dvec3 secondary(0.0, 2.0, 0.0);

        int numColumns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(argc - 1))));

        // root of the scene graph that will contain all nodes.
        auto vsg_scene = vsg::Group::create();

        // Assign any ResourceHints so that the Compile traversal can allocate suffucient DescriptorPool resources for the needs of loading all possible models.
        if (resourceHints)
        {
            vsg_scene->setObject("ResourceHints", resourceHints);
        }

        for (int i = 1; i < argc; ++i)
        {
            vsg::Path filename = argv[i];

            int index = i - 1;
            vsg::dvec3 position = origin + primary * static_cast<double>(index % numColumns) + secondary * static_cast<double>(index / numColumns);
            auto transform = vsg::MatrixTransform::create(vsg::translate(position));

            transform->setValue("filename", filename);

            vsg_scene->addChild(transform);

            filenameAttachments.push_back({filename, transform});
        }

        for (auto& [filename, attachment] : filenameAttachments)
        {
            std::cout << filename << " " << attachment << std::endl;

            auto model = vsg::read_cast<vsg::Node>(filename, options);
            if (model)
            {
                vsg::ComputeBounds computeBounds;
                model->accept(computeBounds);

                vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
                double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;
                auto scale = vsg::MatrixTransform::create(vsg::scale(1.0 / radius, 1.0 / radius, 1.0 / radius) * vsg::translate(-centre));

                scale->addChild(model);
                attachment->addChild(scale);
            }
        }

        // vsg::write(vsg_scene, "test.vsgt");

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

        for (auto& task : viewer->recordAndSubmitTasks)
        {
            std::cout << task << std::endl;
            std::cout << "  windows.size() = " << task->windows.size() << std::endl;
            for (auto& window : task->windows)
            {
                std::cout << "    " << window << std::endl;
            }

            std::cout << "  waitSemaphores.size() " << task->waitSemaphores.size() << std::endl;
            for (auto& semaphore : task->waitSemaphores)
            {
                std::cout << "    " << semaphore << std::endl;
            }

            std::cout << "  commandGraphs.size() = " << task->commandGraphs.size() << std::endl;
            for (auto& cg : task->commandGraphs)
            {
                std::cout << "    " << cg << std::endl;
            }

            std::cout << "  signalSemaphores.size() = " << task->signalSemaphores.size() << std::endl;
            for (auto& semaphore : task->signalSemaphores)
            {
                std::cout << "    " << semaphore << std::endl;
            }
        }

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
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
