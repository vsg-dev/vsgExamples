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

class InsertStateSwitch : public vsg::Visitor
{
public:
    std::vector<vsg::Object*> parents;
    std::set<vsg::Object*> visited;
    std::map<vsg::BindGraphicsPipeline*, vsg::ref_ptr<vsg::StateSwitch>> pipelineMap;
    vsg::Mask mask_1 = 0x1;
    vsg::Mask mask_2 = 0x2;

    void traverse(vsg::Object& object)
    {
        parents.push_back(&object);
        object.traverse(*this);
        parents.pop_back();
    }

    void apply(vsg::Object& object) override
    {
        traverse(object);
    }

    vsg::ref_ptr<vsg::GraphicsPipeline> createAlternate(vsg::GraphicsPipeline& pipeline)
    {
        auto alternative_pipeline = vsg::GraphicsPipeline::create();

        *alternative_pipeline = pipeline;

        for (auto& pipelineState : alternative_pipeline->pipelineStates)
        {
            if (auto rasterizationState = pipelineState.cast<vsg::RasterizationState>())
            {
                auto alternate_rasterizationState = vsg::RasterizationState::create(*rasterizationState);

                alternate_rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;
                pipelineState = alternate_rasterizationState;
            }
        }
        return alternative_pipeline;
    }

    void apply(vsg::StateGroup& sg) override
    {
        if (visited.count(&sg) > 0) return;
        visited.insert(&sg);

        for (auto& sc : sg.stateCommands)
        {
            if (auto bgp = sc->cast<vsg::BindGraphicsPipeline>())
            {
                auto& stateSwitch = pipelineMap[bgp];

                if (!stateSwitch)
                {
                    stateSwitch = vsg::StateSwitch::create();
                    stateSwitch->slot = bgp->slot;
                    stateSwitch->add(mask_1, sc);

                    auto alternate_gp = createAlternate(*(bgp->pipeline));
                    auto alternate_bgp = vsg::BindGraphicsPipeline::create(alternate_gp);

                    stateSwitch->add(mask_2, alternate_bgp);
                }
                sc = stateSwitch;
            }
        }

        traverse(sg);
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "Multiple Views";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    bool insertStateSwitch = !arguments.read("-n"); // no replacement of GraphicsPipeline, so assume loaded scene graph has required vsg::StateSwitch
    bool separateRenderGraph = arguments.read("-s");
    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    vsg::Path filename = "models/lz.vsgt";
    if (argc > 1) filename = arguments[1];
    auto scenegraph = vsg::read_cast<vsg::Node>(filename, options);
    if (!scenegraph)
    {
        std::cout << "Please specify a valid model on command line" << std::endl;
        return 1;
    }

    vsg::Mask mask_1 = 0x1;
    vsg::Mask mask_2 = 0x2;

    // insert StateSwitch in place of BindGraphicsPipeline,
    // with each StateSwitch having one child associated with view_1 that is the original Bind/GraphicsPipeline
    // and a second child associated with view_2 that duplicates the original Bind/GraphicsPipeline to create one with wireframe enabled
    if (insertStateSwitch)
    {
        InsertStateSwitch rgp;
        rgp.mask_1 = mask_1;
        rgp.mask_2 = mask_2;
        scenegraph->accept(rgp);
    }

    if (outputFilename)
    {
        vsg::write(scenegraph, outputFilename, options);
        return 0;
    }

    // enable wireframe polygon mode
    auto requestFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
    requestFeatures->get().fillModeNonSolid = VK_TRUE;

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
    auto main_camera = createCameraForScene(scenegraph, 0, 0, width / 2, height);
    auto main_view = vsg::View::create(main_camera, scenegraph);
    main_view->mask = mask_1;

    // create a RenderGraph to add a secondary vsg::View on the top right part of the window.
    auto secondary_camera = createCameraForScene(scenegraph, width / 2, 0, width / 2, height);
    auto secondary_view = vsg::View::create(secondary_camera, scenegraph);
    secondary_view->mask = mask_2;

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add event handlers, in the order we wish events to be handled.
    viewer->addEventHandler(vsg::Trackball::create(secondary_camera));
    viewer->addEventHandler(vsg::Trackball::create(main_camera));

    if (separateRenderGraph)
    {
        std::cout << "Using a RenderGraph per View" << std::endl;
        auto main_RenderGraph = vsg::RenderGraph::create(window, main_view);
        auto secondary_RenderGraph = vsg::RenderGraph::create(window, secondary_view);
        secondary_RenderGraph->clearValues[0].color = vsg::sRGB_to_linear(0.2f, 0.2f, 0.2f, 1.0f);

        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(main_RenderGraph);
        commandGraph->addChild(secondary_RenderGraph);

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    }
    else
    {
        std::cout << "Using a single RenderGraph, with both Views separated by a ClearAttachments" << std::endl;
        auto renderGraph = vsg::RenderGraph::create(window);

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

        renderGraph->addChild(secondary_view);

        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(renderGraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    }

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
