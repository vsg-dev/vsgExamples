/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield
Copyright(c) 2020 Tim Moore

Portions derived from code that is Copyright (C) Sascha Willems - www.saschawillems.de

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

// Render a scene to an image, then use the image as a texture on the
// faces of quads. This is based on Sascha Willems' offscreenrender
// example.
//
// In VSG / Vulkan terms, we first create a frame buffer that uses
// the image as an attachment. Next, create a RenderGraph that uses that
// frame buffer and the scene graph for the scene. Another RenderGraph
// contains the scene graph for the quads. The quads use the rendered
// image's descriptor. Finally, create the command graph that records those
// two render graphs.
//
// In order for this to work correctly in Vulkan, the subpass
// dependencies of the offscreen RenderPass / RenderGraph need to be
// set up so that the second RenderGraph can sample the output.

// RenderGraph for rendering to image


namespace vsg
{
    class TraverseChildrenOfNode : public vsg::Inherit<vsg::Node, TraverseChildrenOfNode>
    {
    public:

        explicit TraverseChildrenOfNode(vsg::ref_ptr<vsg::Node> in_node) : node(in_node) {}

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
}

vsg::ref_ptr<vsg::RenderGraph> createOffscreenRendergraph(vsg::Context& context, const VkExtent2D& extent,
                                                          vsg::ImageInfo& colorImageInfo, vsg::ImageInfo& depthImageInfo)
{
    auto device = context.device;

    VkExtent3D attachmentExtent{extent.width, extent.height, 1};
    // Attachments
    // create image for color attachment
    auto colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = VK_FORMAT_R8G8B8A8_UNORM;
    colorImage->extent = attachmentExtent;
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = VK_SAMPLE_COUNT_1_BIT;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    auto colorImageView = createImageView(context, colorImage, VK_IMAGE_ASPECT_COLOR_BIT);

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
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    auto depthImage = vsg::Image::create();
    depthImage->imageType = VK_IMAGE_TYPE_2D;
    depthImage->extent = attachmentExtent;
    depthImage->mipLevels = 1;
    depthImage->arrayLayers = 1;
    depthImage->samples = VK_SAMPLE_COUNT_1_BIT;
    depthImage->format = depthFormat;
    depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImage->flags = 0;
    depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // XXX Does layout matter?
    depthImageInfo.sampler = nullptr;
    depthImageInfo.imageView = vsg::createImageView(context, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // attachment descriptions
    vsg::RenderPass::Attachments attachments(2);
    // Color attachment
    attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // Depth attachment
    attachments[1].format = depthFormat;
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

    auto renderPass = vsg::RenderPass::create(device, attachments, subpassDescription, dependencies);

    // Framebuffer
    auto fbuf = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageInfo.imageView, depthImageInfo.imageView}, extent.width, extent.height, 1);

    auto rendergraph = vsg::RenderGraph::create();
    rendergraph->renderArea.offset = VkOffset2D{0, 0};
    rendergraph->renderArea.extent = extent;
    rendergraph->framebuffer = fbuf;

    rendergraph->clearValues.resize(2);
    rendergraph->clearValues[0].color = {{0.4f, 0.2f, 0.4f, 1.0f}};
    rendergraph->clearValues[1].depthStencil = VkClearDepthStencilValue{0.0f, 0};

    return rendergraph;
}

vsg::ref_ptr<vsg::Node> createQuad(const vsg::vec3& position, const vsg::vec2& size, vsg::ref_ptr<vsg::ImageInfo> colorImage)
{
    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return vsg::ref_ptr<vsg::Node>();
    }

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

    // create texture image and associated DescriptorSets and binding
    auto texture = vsg::DescriptorImage::create(colorImage, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create({
         position + vsg::vec3(0.0f, 0.0f, 0.0f),
         position + vsg::vec3(size.x, 0.0f, 0.0f),
         position + vsg::vec3(size.x, size.y, 0.0f),
         position + vsg::vec3(0.0f, size.y, 0.0f)
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
        {
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f}
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}
        }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);
    return scenegraph;
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

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "rendertotexture";
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    windowTraits->synchronizationLayer = arguments.read("--sync");
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }

    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    auto nearFarRatio = arguments.value<double>(0.001, "--nfr");
    bool nestedCommandGraph = arguments.read({"-n", "--nested"});
    bool separateCommandGraph = arguments.read("-s");
    auto numFrames = arguments.value(-1, "-f");

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
    viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel));

    auto view3D = vsg::View::create(camera, vsg_scene);

#if 1

    vsg::vec3 position(0.0f, 0.0f, 0.0f);
    vsg::vec2 size(1.0f, 1.0f);

    auto quads = vsg::Group::create();

    auto context = vsg::Context::create(window->getOrCreateDevice());

    // Framebuffer with attachments
    VkExtent2D targetExtent{512, 512};
    auto offscreenCamera = createCameraForScene(vsg_scene, targetExtent);
    auto colorImage = vsg::ImageInfo::create();
    auto depthImage = vsg::ImageInfo::create();
    auto rtt_RenderGraph = createOffscreenRendergraph(*context, targetExtent, *colorImage, *depthImage);

    auto tcon = vsg::TraverseChildrenOfNode::create(view3D);
    auto rtt_view = vsg::View::create(offscreenCamera, tcon);

    rtt_RenderGraph->addChild(rtt_view);

    // Planes geometry that uses the rendered scene as a texture map
    quads->addChild( createQuad(position, size, colorImage) );

    position.x += size.x * 1.1f;
    quads->addChild( createQuad(position, size, colorImage) );

    position.x += size.x * 1.1f;
    quads->addChild( createQuad(position, size, colorImage) );

    auto quads_camera = createCameraForQuads(quads, window->extent2D());
    auto quads_view = vsg::View::create(quads_camera, quads);

    auto rtt_commandGraph = vsg::CommandGraph::create(window);
    rtt_commandGraph->submitOrder = -1; // render before the main_commandGraph
    rtt_commandGraph->addChild(rtt_RenderGraph);

    auto main_RenderGraph = vsg::createRenderGraphForView(window, camera, vsg_scene);
    main_RenderGraph->addChild(quads_view);

    auto main_commandGraph = vsg::CommandGraph::create(window);
    main_commandGraph->addChild(rtt_commandGraph); // rtt_commandGraph nested within main CommandGraph
    main_commandGraph->addChild(main_RenderGraph);

    viewer->assignRecordAndSubmitTaskAndPresentation({main_commandGraph});
#else
    auto main_commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({main_commandGraph});
#endif

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

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
