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

class ViewHandler : public vsg::Inherit<vsg::Visitor, ViewHandler>
{
public:
    vsg::ref_ptr<vsg::RenderGraph> renderGraph;

    ViewHandler(vsg::ref_ptr<vsg::RenderGraph> in_renderGraph) :
        renderGraph(in_renderGraph) {}

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase == 's')
        {
            keyPress.handled = true;

            std::vector<std::pair<size_t, vsg::ref_ptr<vsg::View>>> views;
            for (size_t i = 0; i < renderGraph->children.size(); ++i)
            {
                if (auto view = renderGraph->children[i].cast<vsg::View>()) views.emplace_back(i, view);
            }

            if (views.size() < 2) return;

            auto renderPass = renderGraph->getRenderPass();
            if (!renderPass) return;

            auto view0 = views[0].second;
            auto viewport0 = view0->camera->getViewport();
            VkExtent2D extent0{static_cast<uint32_t>(viewport0.width), static_cast<uint32_t>(viewport0.height)};

            auto view1 = views[1].second;
            auto viewport1 = view1->camera->getViewport();
            VkExtent2D extent1{static_cast<uint32_t>(viewport1.width), static_cast<uint32_t>(viewport1.height)};

            // swap rendering order by swapping the view entries in the renderGraph->children
            std::swap(renderGraph->children[views[0].first], renderGraph->children[views[1].first]);
            std::swap(views[0].second->camera->viewportState, views[1].second->camera->viewportState);

            // change the aspect ratios of the projection matrices to fit the new dimensions.
            view0->camera->projectionMatrix->changeExtent(extent0, extent1);
            view1->camera->projectionMatrix->changeExtent(extent1, extent0);
        }
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Multiple Views";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    // set up instrumentation if required
    bool reportAverageFrameRate = arguments.read("--fps");
    auto logFilename = arguments.value<vsg::Path>("", "--log");
    vsg::ref_ptr<vsg::Instrumentation> instrumentation;
    if (arguments.read({"--gpu-annotation", "--ga"}) && vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        windowTraits->debugUtils = true;

        auto gpu_instrumentation = vsg::GpuAnnotation::create();
        if (arguments.read("--name"))
            gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_name;
        else if (arguments.read("--className"))
            gpu_instrumentation->labelType = vsg::GpuAnnotation::Object_className;
        else if (arguments.read("--func"))
            gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_function;

        instrumentation = gpu_instrumentation;
    }
    else if (arguments.read({"--profiler", "--pr"}))
    {
        // set Profiler options
        auto settings = vsg::Profiler::Settings::create();
        arguments.read("--cpu", settings->cpu_instrumentation_level);
        arguments.read("--gpu", settings->gpu_instrumentation_level);
        arguments.read("--log-size", settings->log_size);

        // create the profiler
        instrumentation = vsg::Profiler::create(settings);
    }

    if (arguments.read({"-t", "--test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        reportAverageFrameRate = true;
    }

    vsg::ref_ptr<vsg::ResourceHints> resourceHints;
    if (auto resourceHintsFilename = arguments.value<vsg::Path>("", "--rh"))
    {
        resourceHints = vsg::read_cast<vsg::ResourceHints>(resourceHintsFilename, options);
    }

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

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

    // create the vsg::RenderGraph and associated vsg::View
    auto main_camera = createCameraForScene(scenegraph, 0, 0, width, height);
    auto main_view = vsg::View::create(main_camera, scenegraph);

    // create another RenderGraph to add a secondary vsg::View on the top right part of the window.
    auto secondary_camera = createCameraForScene(scenegraph2, (width * 3) / 4, 0, width / 4, height / 4);
    auto secondary_view = vsg::View::create(secondary_camera, scenegraph2);

    // add headlights to views to make sure any objects that need lighting have it.
    auto headlight = vsg::createHeadlight();
    main_view->addChild(headlight);
    secondary_view->addChild(headlight);

    auto renderGraph = vsg::RenderGraph::create(window);

    // add main view that covers the whole window.
    renderGraph->addChild(main_view);

    // clear the depth buffer before view2 gets rendered
    VkClearValue colorClearValue{};
    colorClearValue.color = vsg::sRGB_to_linear(0.2f, 0.2f, 0.2f, 1.0f);
    VkClearAttachment color_attachment{VK_IMAGE_ASPECT_COLOR_BIT, 0, colorClearValue};

    VkClearValue depthClearValue{};
    depthClearValue.depthStencil = {0.0f, 0};
    VkClearAttachment depth_attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, depthClearValue};

    VkClearRect rect{secondary_camera->getRenderArea(), 0, 1};
    auto clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{color_attachment, depth_attachment}, vsg::ClearAttachments::Rects{rect, rect});
    renderGraph->addChild(clearAttachments);

    // add the second insert view that overlays ontop.
    renderGraph->addChild(secondary_view);

    auto commandGraph = vsg::CommandGraph::create(window);
    commandGraph->addChild(renderGraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // add the view handler for interactively changing the views
    viewer->addEventHandler(ViewHandler::create(renderGraph));

    // add close handler to respond to the close window button and to pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add event handlers, in the order we wish events to be handled.
    viewer->addEventHandler(vsg::Trackball::create(secondary_camera));
    viewer->addEventHandler(vsg::Trackball::create(main_camera));

    if (instrumentation) viewer->assignInstrumentation(instrumentation);

    viewer->compile(resourceHints);

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    if (reportAverageFrameRate)
    {
        double fps = static_cast<double>(viewer->getFrameStamp()->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
        std::cout << "Average frame rate = " << fps << " fps" << std::endl;
    }

    if (auto profiler = instrumentation.cast<vsg::Profiler>())
    {
        instrumentation->finish();
        if (logFilename)
        {
            std::ofstream fout(logFilename);
            profiler->log->report(fout);
        }
        else
        {
            profiler->log->report(std::cout);
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
