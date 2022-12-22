#include <vsg/all.h>

#include <chrono>
#include <iostream>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

class CustomImageMemoryBarrierCmd : public vsg::Inherit<vsg::Command, CustomImageMemoryBarrierCmd>
{
public:
    struct ImageMemoryBarrier
    {
        VkPipelineStageFlags src_stage_mask{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};

        VkPipelineStageFlags dst_stage_mask{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};

        VkAccessFlags src_access_mask{0};

        VkAccessFlags dst_access_mask{0};

        VkImageLayout old_layout{VK_IMAGE_LAYOUT_UNDEFINED};

        VkImageLayout new_layout{VK_IMAGE_LAYOUT_UNDEFINED};

        uint32_t old_queue_family{VK_QUEUE_FAMILY_IGNORED};

        uint32_t new_queue_family{VK_QUEUE_FAMILY_IGNORED};
    };

    CustomImageMemoryBarrierCmd(vsg::ref_ptr<vsg::Window> in_window, ImageMemoryBarrier imageMemoryBarrier) :
        _window{in_window}, _mageMemoryBarrier{imageMemoryBarrier}
    {
    }

    void record(vsg::CommandBuffer& commandBuffer) const override {
        size_t imageIndex = _window->imageIndex();
        if (imageIndex >= _window->numFrames()) return;
        auto imageView = _window->imageView(imageIndex);
        VkImageMemoryBarrier image_memory_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        image_memory_barrier.oldLayout = _mageMemoryBarrier.old_layout;
        image_memory_barrier.newLayout = _mageMemoryBarrier.new_layout;
        image_memory_barrier.image = imageView->image->vk(commandBuffer.deviceID);
        image_memory_barrier.subresourceRange = imageView->subresourceRange;
        image_memory_barrier.srcAccessMask = _mageMemoryBarrier.src_access_mask;
        image_memory_barrier.dstAccessMask = _mageMemoryBarrier.dst_access_mask;
        image_memory_barrier.srcQueueFamilyIndex = _mageMemoryBarrier.old_queue_family;
        image_memory_barrier.dstQueueFamilyIndex = _mageMemoryBarrier.new_queue_family;

        VkPipelineStageFlags src_stage_mask = _mageMemoryBarrier.src_stage_mask;
        VkPipelineStageFlags dst_stage_mask = _mageMemoryBarrier.dst_stage_mask;

        vkCmdPipelineBarrier(commandBuffer.vk(), src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
    }

    ImageMemoryBarrier _mageMemoryBarrier;
    vsg::ref_ptr<vsg::Window> _window;
};

vsg::AttachmentDescription customColorAttachment(VkFormat imageFormat)
{
    vsg::AttachmentDescription colorAttachment = {};
    colorAttachment.format = imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    return colorAttachment;
}

vsg::ref_ptr<vsg::RenderPass> createCustomRenderPass(vsg::Device* device, VkFormat imageFormat, VkFormat depthFormat, bool requiresDepthRead)
{
    auto colorAttachment = customColorAttachment(imageFormat);
    auto depthAttachment = vsg::defaultDepthAttachment(depthFormat);

    if (requiresDepthRead)
    {
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    }

    vsg::RenderPass::Attachments attachments{colorAttachment, depthAttachment};

    vsg::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::AttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachments.emplace_back(colorAttachmentRef);
    subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

    vsg::RenderPass::Subpasses subpasses{subpass};

    // image layout transition
    vsg::SubpassDependency colorDependency = {};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    colorDependency.dependencyFlags = 0;

    // depth buffer is shared between swap chain images
    vsg::SubpassDependency depthDependency = {};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.dstSubpass = 0;
    depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthDependency.dependencyFlags = 0;

    vsg::RenderPass::Dependencies dependencies{colorDependency, depthDependency};

    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}

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

    bool separateRenderGraph = arguments.read("-s");

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
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;

    // create the vsg::RenderGraph and associated vsg::View
    auto main_camera = createCameraForScene(scenegraph, 0, 0, width, height);
    auto main_view = vsg::View::create(main_camera, scenegraph);

    // create an RenderinGraph to add an secondary vsg::View on the top right part of the window.
    auto secondary_camera = createCameraForScene(scenegraph2, (width * 3) / 4, 0, width / 4, height / 4);
    auto secondary_view = vsg::View::create(secondary_camera, scenegraph2);

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // add event handlers, in the order we wish event to be handled.
    viewer->addEventHandler(vsg::Trackball::create(secondary_camera));
    viewer->addEventHandler(vsg::Trackball::create(main_camera));

    if (separateRenderGraph)
    {
        CustomImageMemoryBarrierCmd::ImageMemoryBarrier beginBarrier;
        beginBarrier.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        beginBarrier.new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        beginBarrier.src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        beginBarrier.src_access_mask = VK_ACCESS_NONE;
        beginBarrier.dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        beginBarrier.dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        CustomImageMemoryBarrierCmd::ImageMemoryBarrier endBarrier;
        endBarrier.old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        endBarrier.new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        endBarrier.src_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        endBarrier.src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        endBarrier.dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        endBarrier.dst_access_mask = VK_ACCESS_MEMORY_READ_BIT;

        std::cout << "Using a RenderGraph per View" << std::endl;
        auto main_RenderGraph = vsg::RenderGraph::create(window, main_view);
        auto secondary_RenderGraph = vsg::RenderGraph::create(window, secondary_view);
        secondary_RenderGraph->clearValues[0].color = {{1.0f, 0.0f, 0.0f, 1.0f}};

        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(CustomImageMemoryBarrierCmd::create(window, beginBarrier));
        commandGraph->addChild(main_RenderGraph);
        commandGraph->addChild(secondary_RenderGraph);
        commandGraph->addChild(CustomImageMemoryBarrierCmd::create(window, endBarrier));
     
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    }
    else
    {
        std::cout << "Using a single RenderGraph, with both Views separated by a ClearAttachments" << std::endl;
        auto renderGraph = vsg::RenderGraph::create(window);

        renderGraph->addChild(main_view);

        // clear the depth buffer before view2 gets rendered

        VkClearValue colorClearValue{};
        colorClearValue.color = {{0.2f, 0.2f, 0.2f, 1.0f}};
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
    if (separateRenderGraph) {
        window->setRenderPass(createCustomRenderPass(window->getDevice(), window->surfaceFormat().format,
                                                     window->depthFormat(),
                                                     false));
    }

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
