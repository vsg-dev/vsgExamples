#include <vsg/all.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
                                      centre, vsg::dvec3(0.0, 0.0, 1.0));

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                                nearFarRatio * radius, radius * 4.5);

    auto viewportstate = vsg::ViewportState::create(x, y, width, height);

    return vsg::Camera::create(perspective, lookAt, viewportstate);
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Multiple Views";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    bool seperateDevices = arguments.read({"--no-shared-window", "-n"});

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    options->readOptions(arguments);

    if (seperateDevices && VSG_MAX_DEVICES<2)
    {
        std::cout<<"VulkanSceneGraph built with VSG_MAX_DEVICES = "<<VSG_MAX_DEVICES<<", so using a two windows, with a vsg::Device per vsg::Window is not supported."<<std::endl;
        return 1;
    }

    vsg::ref_ptr<vsg::Node> scenegraph;
    vsg::ref_ptr<vsg::Node> scenegraph2;
    if (argc > 1)
    {
        vsg::Path filename = arguments[1];
        scenegraph = vsg::read_cast<vsg::Node>(filename, options);
        scenegraph2 = scenegraph;
    }

    if (argc > 2)
    {
        vsg::Path filename = arguments[2];
        scenegraph2 = vsg::read_cast<vsg::Node>(filename, options);
    }

    if (!scenegraph || !scenegraph2)
    {
        std::cout << "Please specify a valid model on command line" << std::endl;
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window1 = vsg::Window::create(windowTraits);
    if (!window1)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    auto windowTraits2 = vsg::WindowTraits::create();
    windowTraits2->windowTitle = "second window";
    windowTraits2->debugLayer = windowTraits->debugLayer;
    windowTraits2->apiDumpLayer = windowTraits->apiDumpLayer;
    if (!seperateDevices)
    {
        windowTraits2->shareWindow = window1; // share the same vsg::Instance/vsg::Device as window1
        std::cout<<"Sharing vsg::Instance and vsg::Device between windows."<<std::endl;
    }
    else
    {
        std::cout<<"Each window to use it's own vsg::Instance and vsg::Device."<<std::endl;
    }
    auto window2 = vsg::Window::create(windowTraits2);
    if (!window2)
    {
        std::cout << "Could not create second window." << std::endl;
        return 1;
    }

    viewer->addWindow(window1);
    viewer->addWindow(window2);

    uint32_t width = window1->extent2D().width;
    uint32_t height = window1->extent2D().height;

    // create the vsg::RenderGraph and associated vsg::View
    auto main_camera = createCameraForScene(scenegraph, 0, 0, width, height);
    auto main_view = vsg::View::create(main_camera, scenegraph);

    // create an RenderinGraph to add an secondary vsg::View on the top right part of the window.
    auto secondary_camera = createCameraForScene(scenegraph2, 0, 0, width, height);
    auto secondary_view = vsg::View::create(secondary_camera, scenegraph2);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add event handlers, in the order we wish event to be handled.

    auto main_trackball = vsg::Trackball::create(main_camera);
    main_trackball->addWindow(window1);
    viewer->addEventHandler(main_trackball);

    auto secondary_trackball = vsg::Trackball::create(secondary_camera);
    secondary_trackball->addWindow(window2);
    viewer->addEventHandler(secondary_trackball);

    auto main_RenderGraph = vsg::RenderGraph::create(window1, main_view);
    auto secondary_RenderGraph = vsg::RenderGraph::create(window2, secondary_view);
    secondary_RenderGraph->clearValues[0].color = {{0.2f, 0.2f, 0.2f, 1.0f}};

    auto commandGraph1 = vsg::CommandGraph::create(window1);
    commandGraph1->addChild(main_RenderGraph);

    auto commandGraph2 = vsg::CommandGraph::create(window2);
    commandGraph2->addChild(secondary_RenderGraph);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph1, commandGraph2});

    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
