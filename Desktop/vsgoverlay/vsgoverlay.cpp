#include <vsg/all.h>

#include <iostream>
#include <chrono>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#endif

namespace vsg
{
    class ClearAttachments : public Inherit<Command, ClearAttachments>
    {
    public:

        using Attachments = std::vector<VkClearAttachment>;
        using Rects = std::vector<VkClearRect>;

        ClearAttachments() {}

        ClearAttachments(const Attachments& in_attachments, const Rects& in_rects) :
            attachments(in_attachments),
            rects(in_rects) {}

        Attachments attachments;
        Rects rects;

        void record(CommandBuffer& commandBuffer) const override
        {
            vkCmdClearAttachments(commandBuffer,
                                static_cast<uint32_t>(attachments.size()), attachments.data(),
                                static_cast<uint32_t>(rects.size()), rects.data());
        }
    };
};

vsg::ref_ptr<vsg::RenderPass> createRenderPass( vsg::Device* device)
{
    VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    // VkAttachmentDescriptiom
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkAttachmentDescription.html

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::RenderPass::Attachments attachments{colorAttachment, depthAttachment};

    // VkSubpassDescription
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkSubpassDescription.html

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::SubpassDescription depth_subpass;
    depth_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    depth_subpass.colorAttachments.emplace_back(colorAttachmentRef);
    depth_subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

    vsg::SubpassDescription classic_subpass;
    classic_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    classic_subpass.colorAttachments.emplace_back(colorAttachmentRef);
    classic_subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

    vsg::RenderPass::Subpasses subpasses{depth_subpass, classic_subpass};

    // VkSubpassDependency
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkSubpassDependency.html

    VkSubpassDependency classic_dependency = {};
    classic_dependency.srcSubpass = 0;
    classic_dependency.dstSubpass = 1;
    classic_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    classic_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    classic_dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    classic_dependency.dstAccessMask = 0;
    classic_dependency.dependencyFlags = 0;

    vsg::RenderPass::Dependencies dependencies{classic_dependency};

    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0),
                                      centre, vsg::dvec3(0.0, 0.0, 1.0));

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(width) / static_cast<double>(height),
                                                nearFarRatio*radius, radius * 4.5);

    VkViewport viewport{static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    VkRect2D scissor{VkOffset2D{x, y}, VkExtent2D{width, height}};

    auto viewportstate = vsg::ViewportState::create();
    viewportstate->viewports.push_back(viewport);
    viewportstate->scissors.push_back(scissor);

    return vsg::Camera::create(perspective, lookAt, viewportstate);
}


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "rendertotexture";
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    bool separateCommandGraph = arguments.read("-s");
    bool multiThreading = arguments.read("--mt");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto options = vsg::Options::create();
#ifdef USE_VSGXCHANGE
    // add use of vsgXchange's support for reading and writing 3rd party file formats
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif

    vsg::ref_ptr<vsg::Node> scenegraph;
    vsg::ref_ptr<vsg::Node> scenegraph2;
    if (argc>1)
    {
        vsg::Path filename = arguments[1];
        scenegraph = vsg::read_cast<vsg::Node>(filename, options);
        scenegraph2 = vsg::read_cast<vsg::Node>(filename, options);
    }

    if (argc>2)
    {
        vsg::Path filename = arguments[2];
        scenegraph2 = vsg::read_cast<vsg::Node>(filename, options);
    }

    if (!scenegraph)
    {
        std::cout<<"Please specify a valid model on command line"<<std::endl;
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // provide a custom RenderPass
    // window->setRenderPass(createRenderPass(window->getOrCreateDevice()));

    uint32_t width = window->extent2D().width;
    uint32_t height = window->extent2D().height;

    auto camera = createCameraForScene(scenegraph, 0, 0, width, height);

    //renderGraph->addChild(vsg::NextSubPass::create(VK_SUBPASS_CONTENTS_INLINE));

#if 1
    auto renderGraph = vsg::createRenderGraphForView(window, {}, {});

    // create view1
    auto view1 = vsg::View::create();
    view1->camera = camera;
    view1->addChild(scenegraph);
    renderGraph->addChild(view1);


    // create view2
    auto view2 = vsg::View::create();
    view2->camera = createCameraForScene(scenegraph2, 0, 0, width, height);

    VkClearAttachment attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, VkClearValue{1.0f, 0.0f}};
    VkClearRect rect{VkRect2D{VkOffset2D{0, 0}, VkExtent2D{width, height}}, 0, 1};
    view2->addChild(vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{attachment}, vsg::ClearAttachments::Rects{rect}));

    view2->addChild(scenegraph2);

    renderGraph->addChild(view2);
#else

    auto renderGraph = vsg::createRenderGraphForView(window, camera, scenegraph);

    renderGraph->addChild(vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{attachment}, vsg::ClearAttachments::Rects{rect}));
    renderGraph->addChild(scenegraph2);
#endif

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::CommandGraph::create(window);

    commandGraph->addChild(renderGraph);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

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
