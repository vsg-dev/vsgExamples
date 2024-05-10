#include <vsg/all.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    // compute the bounds of the scene graph to help position the camera
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
    windowTraits->windowTitle = "Multiple Render Graphs";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

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
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);
    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;

    // add close handler to respond to the close window button and to pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto commandGraph = vsg::CommandGraph::create(window);

    // create the vsg::RenderGraph and associated vsg::View
    auto main_camera = createCameraForScene(scenegraph, 0, 0, width, height);
    {
        // add main view that covers the whole window.
        auto main_view = vsg::View::create(main_camera, scenegraph);
        auto main_renderGraph = vsg::RenderGraph::create(window);
        main_renderGraph->addChild(main_view);
        commandGraph->addChild(main_renderGraph);
    }

    // create another RenderGraph to add a secondary vsg::View on the top right part of the window.
    auto secondary_camera = createCameraForScene(scenegraph2, (width * 3) / 4, 0, width / 4, height / 4);
    {
        // add the second insert view that overlays ontop.
        auto secondary_view = vsg::View::create(secondary_camera, scenegraph2);
        auto secondary_renderGraph = vsg::RenderGraph::create(window);
        secondary_renderGraph->clearValues.clear();
        secondary_renderGraph->setClearValues(
          VkClearColorValue{{0.2f, 0.2f, 0.2f, 0.0f}},
          VkClearDepthStencilValue{0.0f, 0});
        secondary_renderGraph->renderArea = secondary_camera->getRenderArea();
        secondary_renderGraph->addChild(secondary_view);
        commandGraph->addChild(secondary_renderGraph);
    }

    // add event handlers, in the order we wish events to be handled.
    viewer->addEventHandler(vsg::Trackball::create(secondary_camera));
    viewer->addEventHandler(vsg::Trackball::create(main_camera));

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }
}
