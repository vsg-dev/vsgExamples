/* <editor-fold desc="MIT License">

Copyright(c) 2023 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <chrono>
#include <iostream>
#include <thread>

namespace vsg
{
    class TraverseChildrenOfNode : public vsg::Inherit<vsg::Node, TraverseChildrenOfNode>
    {
    public:
        explicit TraverseChildrenOfNode(vsg::ref_ptr<vsg::Node> in_node) :
            node(in_node) {}

        vsg::observer_ptr<vsg::Node> node;

        template<class N, class V>
        static void t_traverse(N& in_node, V& visitor)
        {
            if (auto ref_node = in_node.node.ref_ptr()) ref_node->traverse(visitor);
        }

        void traverse(Visitor& visitor) override { t_traverse(*this, visitor); }
        void traverse(ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
        void traverse(RecordTraversal& visitor) const override { t_traverse(*this, visitor); }
    };
    VSG_type_name(vsg::TraverseChildrenOfNode);
} // namespace vsg

vsg::ref_ptr<vsg::Image> createImage(vsg::Context& context, uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage)
{
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{width, height, 1};
    image->mipLevels = 1;
    image->arrayLayers = levels;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->flags = 0;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // allocte vkImage and required memory
    image->compile(context);

    return image;
}

vsg::ref_ptr<vsg::RenderGraph> createOffscreenRendergraph(vsg::Context& context, const VkExtent2D& extent, uint32_t layer,
                                                          vsg::ref_ptr<vsg::Image> colorImage, vsg::ImageInfo& colorImageInfo,
                                                          vsg::ref_ptr<vsg::Image> depthImage, vsg::ImageInfo& depthImageInfo)
{
    auto colorImageView = vsg::ImageView::create(colorImage, VK_IMAGE_ASPECT_COLOR_BIT);
    colorImageView->subresourceRange.baseArrayLayer = layer;
    colorImageView->subresourceRange.layerCount = 1;
    colorImageView->compile(context);

    // Sampler for accessing attachment as a texture
    auto colorSampler = vsg::Sampler::create();
    colorSampler->flags = 0;
    colorSampler->magFilter = VK_FILTER_LINEAR;
    colorSampler->minFilter = VK_FILTER_LINEAR;
    colorSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    colorSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    colorSampler->addressModeV = colorSampler->addressModeU;
    colorSampler->addressModeW = colorSampler->addressModeU;
    colorSampler->mipLodBias = 0.0f;
    colorSampler->maxAnisotropy = 1.0f;
    colorSampler->minLod = 0.0f;
    colorSampler->maxLod = 1.0f;
    colorSampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    colorImageInfo.imageView = colorImageView;
    colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorImageInfo.sampler = colorSampler;

    // create depth buffer

    auto depthImageView = vsg::ImageView::create(depthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
    depthImageView->subresourceRange.baseArrayLayer = layer;
    depthImageView->subresourceRange.layerCount = 1;
    depthImageView->compile(context);

    depthImageInfo.sampler = nullptr;
    depthImageInfo.imageView = depthImageView;
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // attachment descriptions
    vsg::RenderPass::Attachments attachments(2);
    // Color attachment
    attachments[0].format = colorImage->format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // Depth attachment
    attachments[1].format = depthImage->format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::AttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    vsg::AttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    vsg::RenderPass::Subpasses subpassDescription(1);
    subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[0].colorAttachments.emplace_back(colorReference);
    subpassDescription[0].depthStencilAttachments.emplace_back(depthReference);

    vsg::RenderPass::Dependencies dependencies(2);

    // XXX This dependency is copied from the offscreenrender.cpp
    // example. I don't completely understand it, but I think its
    // purpose is to create a barrier if some earlier render pass was
    // using this framebuffer's attachment as a texture.
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // This is the heart of what makes Vulkan offscreen rendering
    // work: render passes that follow are blocked from using this
    // passes' color attachment in their fragment shaders until all
    // this pass' color writes are finished.
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    auto renderPass = vsg::RenderPass::create(context.device, attachments, subpassDescription, dependencies);

    // Framebuffer
    auto fbuf = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageInfo.imageView, depthImageInfo.imageView}, extent.width, extent.height, 1);

    auto rendergraph = vsg::RenderGraph::create();
    rendergraph->renderArea.offset = VkOffset2D{0, 0};
    rendergraph->renderArea.extent = extent;
    rendergraph->framebuffer = fbuf;

    rendergraph->clearValues.resize(2);
    rendergraph->clearValues[0].color = vsg::sRGB_to_linear(0.4f, 0.2f, 0.4f, 1.0f);
    rendergraph->clearValues[1].depthStencil = VkClearDepthStencilValue{0.0f, 0};

    return rendergraph;
}

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, const VkExtent2D& extent)
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

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height),
                                                nearFarRatio * radius, radius * 4.5);

    return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));
}

vsg::ref_ptr<vsg::Camera> createCameraForQuads(vsg::Node* scenegraph, const VkExtent2D& extent)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, 0.0, radius * 3.5),
                                      centre, vsg::dvec3(0.0, 1.0, 0.0));

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height),
                                                nearFarRatio * radius, radius * 4.5);

    return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));
}

namespace vsg
{
    class EventNode : public vsg::Inherit<vsg::Node, EventNode>
    {
    public:
        EventNode() {}
        explicit EventNode(const EventHandlers& in_eventHandlers) :
            eventHandlers(in_eventHandlers) {}
        explicit EventNode(ref_ptr<Visitor> eventHandler)
        {
            if (eventHandler) eventHandlers.push_back(eventHandler);
        }

        EventHandlers eventHandlers;
    };
} // namespace vsg

struct AssignEventHandlers : public vsg::Visitor
{
    vsg::Viewer& viewer;
    AssignEventHandlers(vsg::Viewer& in_viewer) :
        viewer(in_viewer) {}

    void apply(vsg::Object& object) override { object.traverse(*this); }
    void apply(vsg::Node& node) override
    {
        if (auto eventNode = node.cast<vsg::EventNode>())
        {
            viewer.addEventHandlers(eventNode->eventHandlers);
        }
        else
        {
            node.traverse(*this);
        }
    }
};

vsg::ref_ptr<vsg::Node> createQuad(const vsg::vec3& position, const vsg::vec2& size)
{
    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create({position + vsg::vec3(0.0f, 0.0f, 0.0f),
                                            position + vsg::vec3(size.x, 0.0f, 0.0f),
                                            position + vsg::vec3(size.x, size.y, 0.0f),
                                            position + vsg::vec3(0.0f, size.y, 0.0f)});

    auto colors = vsg::vec3Array::create(
        {{1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}});

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 1.0f},
         {1.0f, 1.0f},
         {1.0f, 0.0f},
         {0.0f, 0.0f}});

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0});

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));
    return drawCommands;
}

vsg::ref_ptr<vsg::CommandGraph> createResultsWindow(vsg::ref_ptr<vsg::Device> device, uint32_t width, uint32_t height, vsg::ImageInfoList& imageInfos, VkPresentModeKHR presentMode)
{
    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

    // create window
    auto resultsWindowTraits = vsg::WindowTraits::create();
    resultsWindowTraits->width = width;
    resultsWindowTraits->height = height;
    resultsWindowTraits->windowTitle = "render to texture results";
    resultsWindowTraits->device = device;
    resultsWindowTraits->swapchainPreferences.presentMode = presentMode;
    auto resultsWindow = vsg::Window::create(resultsWindowTraits);

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()};

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);

    vsg::vec3 position(0.0f, 0.0f, 0.0f);
    vsg::vec2 size(1.0f, 1.0f);

    float aspecRatio = static_cast<float>(width) / static_cast<float>(height);
    uint32_t nx = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(imageInfos.size()) * aspecRatio)));

    uint32_t col = 0;
    for (auto& imageInfo : imageInfos)
    {
        // create texture image and associated DescriptorSets and binding
        auto texture = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, descriptorSet);

        // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
        auto quadgroup = vsg::StateGroup::create();
        quadgroup->add(bindDescriptorSet);
        quadgroup->addChild(createQuad(position, size));

        scenegraph->addChild(quadgroup);

        col++;
        if (col < nx)
        {
            position.x += size.x * 1.1f;
        }
        else
        {
            col = 0;
            position.x = 0.0f;
            position.y += size.y * 1.1f;
        }
    }

    auto quads_camera = createCameraForQuads(scenegraph, resultsWindow->extent2D());
    auto quads_view = vsg::View::create(quads_camera, scenegraph);

    auto results_RenderGraph = vsg::RenderGraph::create(resultsWindow);
    results_RenderGraph->addChild(quads_view);

    auto results_commandGraph = vsg::CommandGraph::create(resultsWindow);
    results_commandGraph->submitOrder = 1;
    results_commandGraph->addChild(results_RenderGraph);

    auto results_trackball = vsg::Trackball::create(quads_camera);
    results_trackball->addWindow(resultsWindow);
    // disable the rotation of the trackball
    results_trackball->rotateButtonMask = vsg::BUTTON_MASK_OFF;
    results_trackball->turnLeftKey = vsg::KEY_Undefined;
    results_trackball->turnRightKey = vsg::KEY_Undefined;
    results_trackball->pitchUpKey = vsg::KEY_Undefined;
    results_trackball->pitchDownKey = vsg::KEY_Undefined;
    results_trackball->rollLeftKey = vsg::KEY_Undefined;
    results_trackball->rollRightKey = vsg::KEY_Undefined;

    results_commandGraph->addChild(vsg::EventNode::create(results_trackball));

    return results_commandGraph;
}

struct CollectStats : public vsg::ConstVisitor
{
    std::map<const vsg::GraphicsPipeline*, uint32_t> pipelines;
    std::map<const vsg::View*, uint32_t> views;

    void apply(const vsg::Object& object) override
    {
        object.traverse(*this);
    }

    void apply(const vsg::BindGraphicsPipeline& bgp) override
    {
        ++pipelines[bgp.pipeline.get()];
    }

    void apply(const vsg::View& view) override
    {
        ++views[&view];
        view.traverse(*this);
    }

    void print(std::ostream& out)
    {
        std::set<uint32_t> viewIDs;

        out << "number of vsg::View " << views.size() << std::endl;
        for (auto& [view, count] : views)
        {
            out << "   view = " << view << ", viewID = " << view->viewID << ", count = " << count << std::endl;
            viewIDs.insert(view->viewID);
        }

        uint32_t numPipelines = 0;

        out << "number of vsg::GraphicsPipelines " << pipelines.size() << std::endl;
        for (auto& [pipeline, count] : pipelines)
        {
            out << "   pipeline = " << pipeline << ", count = " << count;
            for (auto viewID : viewIDs)
            {
                if (pipeline->validated_vk(viewID) != 0)
                {
                    ++numPipelines;
                    out << " pipeline->vk(" << viewID << ") = " << pipeline->validated_vk(viewID);
                }
            }
            out << std::endl;
        }

        out << "number of vkPipelines = " << numPipelines << std::endl;
    }
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "render to texture 2d array";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    windowTraits->synchronizationLayer = arguments.read("--sync");
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
    auto numFrames = arguments.value(-1, "-f");
    auto numLayers = arguments.value<uint32_t>(4, "--numLayers");
    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    if (arguments.read({"-t", "--test"}))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->fullscreen = true;
    }
    auto shareViewID = arguments.value(false, "-s");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto vsg_scene = vsg::Group::create();

    // read any vsg files
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        auto loaded_scene = vsg::read_cast<vsg::Node>(filename, options);
        if (loaded_scene)
        {
            vsg_scene->addChild(loaded_scene);
            arguments.remove(i, 1);
            --i;
        }
    }

    if (vsg_scene->children.empty())
    {
        std::cout << "No valid model files specified." << std::endl;
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

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel");
    if (ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto animationPathHandler = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
    if (animationPathHandler->animation) animationPathHandler->play();
    viewer->addEventHandler(animationPathHandler);

    auto main_trackball = vsg::Trackball::create(camera, ellipsoidModel);
    main_trackball->addWindow(window);
    viewer->addEventHandler(main_trackball);

    auto view3D = vsg::View::create(camera, vsg_scene);

    auto context = vsg::Context::create(window->getOrCreateDevice());

    auto main_RenderGraph = vsg::RenderGraph::create(window);
    main_RenderGraph->addChild(view3D);

    auto main_commandGraph = vsg::CommandGraph::create(window);
    main_commandGraph->submitOrder = 0;
    main_commandGraph->addChild(main_RenderGraph);

    vsg::CommandGraphs commandGraphs;
    commandGraphs.push_back(main_commandGraph);

    if (numLayers > 0)
    {
        vsg::ImageInfoList imageInfos;
        VkExtent2D targetExtent{512, 512};

        // create the color and depth image 2D arrays to render to/read from.
        auto colorImage = createImage(*context, targetExtent.width, targetExtent.height, numLayers, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        auto depthImage = createImage(*context, targetExtent.width, targetExtent.height, numLayers, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

        // use the TraverseChildrenOfNode to decorate the main view3D so it's children can be travesed by the render to texture without invoking the view3D camera/ViewDependentState
        auto tcon = vsg::TraverseChildrenOfNode::create(view3D);

        // create render to textures
        auto rtt_commandGraph = vsg::CommandGraph::create(window);
        rtt_commandGraph->submitOrder = -1;            // render before the main_commandGraph
        main_commandGraph->addChild(rtt_commandGraph); // rtt_commandGraph nested within main CommandGraph
        vsg::ref_ptr<vsg::View> first_rrt_view;
        for (uint32_t layer = 0; layer < numLayers; ++layer)
        {
            // create RenderGraph for render to texture for specified layer
            auto offscreenCamera = createCameraForScene(vsg_scene, targetExtent);
            auto colorImageInfo = vsg::ImageInfo::create();
            auto depthImageInfo = vsg::ImageInfo::create();
            auto rtt_RenderGraph = createOffscreenRendergraph(*context, targetExtent, layer, colorImage, *colorImageInfo, depthImage, *depthImageInfo);

            imageInfos.push_back(colorImageInfo);

            vsg::ref_ptr<vsg::View> rtt_view;
            if (shareViewID)
            {
                if (first_rrt_view)
                {
                    rtt_view = vsg::View::create(*first_rrt_view);
                    rtt_view->camera = offscreenCamera;
                }
                else
                {
                    first_rrt_view = rtt_view = vsg::View::create(offscreenCamera, tcon);
                }
            }
            else
            {
                rtt_view = vsg::View::create(offscreenCamera, tcon);
            }

            rtt_RenderGraph->addChild(rtt_view);
            rtt_commandGraph->addChild(rtt_RenderGraph);
        }

        commandGraphs.push_back(createResultsWindow(window->getOrCreateDevice(), windowTraits->width / 2, windowTraits->height / 2, imageInfos, windowTraits->swapchainPreferences.presentMode));
    }

    viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs);

    // traverse the viewer's commandGraphs to add any event handlers specificied via the EventNode.
    AssignEventHandlers aeh(*viewer);
    for (auto cg : commandGraphs) cg->accept(aeh);

    // compile all the application and scene graph levels Vulkan objects and transfer data to GPU memory.
    viewer->compile();

    // collect and print out scene graph stats
    vsg::visit<CollectStats>(commandGraphs.begin(), commandGraphs.end()).print(std::cout);

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

    auto fs = viewer->getFrameStamp();
    double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
    std::cout << "Average frame rate = " << fps << " fps" << std::endl;

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
