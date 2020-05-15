/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield
Copyright(c) 2020 Tim Moore

Portions derived from code that is Copyright (C) Sascha Willems - www.saschawillems.de

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/all.h>

#include <iostream>
#include <chrono>
#include <thread>

// Descriptor class for an image whose contents don't need to be
// initialized by a transfer to memory.

class SimpleDescriptorImage : public vsg::Inherit<vsg::Descriptor, SimpleDescriptorImage>
{
public:
    // Sampler in _imageData manages per-context state, so this class
    // doesn't need to.
    vsg::ImageData _imageData;
    
    SimpleDescriptorImage(const vsg::ImageData& imageData, uint32_t dstBinding, uint32_t dstArrayElement,
                          VkDescriptorType descriptorType)
        : Inherit(dstBinding, dstArrayElement, descriptorType), _imageData(imageData)
    {}

    void compile(vsg::Context& context) override
    {
        _imageData._sampler->compile(context);
    }

    void assignTo(vsg::Context& context, VkWriteDescriptorSet& wds) const override
    {
        Inherit::assignTo(context, wds);
        auto pImageInfo = context.scratchMemory->allocate<VkDescriptorImageInfo>(1);
        wds.descriptorCount = 1;
        wds.pImageInfo = pImageInfo;
        pImageInfo->imageLayout = _imageData._imageLayout;
        pImageInfo->imageView = *_imageData._imageView;
        pImageInfo->sampler = _imageData._sampler->vk(context.deviceID);
    }
};

// Render a scene to an image, then use the image as a texture on the
// faces of quads. This is based on Sascha William's offscreenrender
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

// Create an image. This should probably be in VSG.
vsg::ImageData createImageView(vsg::Context& context, const VkImageCreateInfo& imageCreateInfo,
                               VkImageAspectFlags aspectFlags, VkImageLayout targetImageLayout)
{
    vsg::Device* device = context.device;

    auto image = vsg::Image::create(device, imageCreateInfo);

    // allocate memory with out export memory info extension
    auto[deviceMemory, offset] = context.deviceMemoryBufferPools->reserveMemory(image->getMemoryRequirements(), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (!deviceMemory)
    {
        std::cout << "Warning: Failed allocate memory to reserve slot" << std::endl;
        return vsg::ImageData();
    }

    image->bind(deviceMemory, offset);

    auto imageview = vsg::ImageView::create(device, image, VK_IMAGE_VIEW_TYPE_2D, imageCreateInfo.format, aspectFlags);

    return vsg::ImageData(nullptr, imageview, targetImageLayout);
}

// Rendergraph for rendering to image

vsg::ref_ptr<vsg::RenderGraph> createOffscreenRendergraph(vsg::Device* device, vsg::Context& context, vsg::Camera* camera,
                                                          const VkExtent2D& extent,
                                                     vsg::ImageData& colorImage, vsg::ImageData& depthImage)
{
    VkExtent3D attachmentExtent{extent.width, extent.height, 1};
    // Attachments
    // create image for color attachment
    VkImageCreateInfo colorImageCreateInfo;
    colorImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    colorImageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorImageCreateInfo.extent = attachmentExtent;
    colorImageCreateInfo.mipLevels = 1;
    colorImageCreateInfo.arrayLayers = 1;
    colorImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    colorImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImageCreateInfo.flags = 0;
    colorImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    colorImageCreateInfo.queueFamilyIndexCount = 0;
    colorImageCreateInfo.pNext = nullptr;
    colorImage = createImageView(context, colorImageCreateInfo,
                                 VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // Sampler for accessing attachment as a texture
    auto colorSampler = vsg::Sampler::create();
    VkSamplerCreateInfo& samplerInfo = colorSampler->info();
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext = nullptr;
    samplerInfo.flags = 0;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    colorImage._sampler = colorSampler;
    // create depth buffer
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    VkImageCreateInfo depthImageCreateInfo = {};
    depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.extent = attachmentExtent;
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.format = depthFormat;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageCreateInfo.flags = 0;
    depthImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageCreateInfo.pNext = nullptr;
    // XXX Does layout matter?
    depthImage = createImageView(context, depthImageCreateInfo,
                                 VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                 VK_IMAGE_LAYOUT_GENERAL);

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

    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    vsg::RenderPass::Subpasses subpassDescription(1);
    subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[0].colorAttachmentCount = 1;
    subpassDescription[0].pColorAttachments = &colorReference;
    subpassDescription[0].pDepthStencilAttachment = &depthReference;

    vsg::RenderPass::Dependencies dependencies(2);

    // XXX This dependency is copied from the offscreenrender.cpp
    // example. I don't completely understand it, but I think it's
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
    VkImageView imageViewAttachments[2];
    imageViewAttachments[0] = *colorImage._imageView;
    imageViewAttachments[1] = *depthImage._imageView;

    VkFramebufferCreateInfo fbufCreateInfo= {};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.renderPass = *renderPass;
    fbufCreateInfo.attachmentCount = 2;
    fbufCreateInfo.pAttachments = imageViewAttachments;
    fbufCreateInfo.width = extent.width;
    fbufCreateInfo.height = extent.height;
    fbufCreateInfo.layers = 1;
    auto fbuf = vsg::Framebuffer::create(device, fbufCreateInfo);

    auto rendergraph = vsg::RenderGraph::create();
    rendergraph->camera = camera;
    rendergraph->renderArea.offset = VkOffset2D{0, 0};
    rendergraph->renderArea.extent = extent;
    rendergraph->renderPass = renderPass;
    rendergraph->framebuffer = fbuf;
    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.4f, 0.2f, 0.2f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };
    rendergraph->clearValues.insert(rendergraph->clearValues.begin(), &clearValues[0], &clearValues[2]);
    return rendergraph;
}

vsg::ref_ptr<vsg::Node> createPlanes(vsg::ImageData& colorImage)
{
    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return vsg::ref_ptr<vsg::Node>();
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    vsg::GraphicsPipelineStates pipelineStates
    {
        vsg::VertexInputState::create( vertexBindingsDescriptions, vertexAttributeDescriptions ),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()
    };

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
#if 0
    auto texture = SimpleDescriptorImage::create(colorImage, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
#else
    auto texture = vsg::DescriptorImageView::create(colorImage, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
#endif

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
    {
        {-0.5f, -0.5f, 0.0f},
        {0.5f,  -0.5f, 0.05f},
        {0.5f , 0.5f, 0.0f},
        {-0.5f, 0.5f, 0.0f},
        {-0.5f, -0.5f, -0.5f},
        {0.5f,  -0.5f, -0.5f},
        {0.5f , 0.5f, -0.5},
        {-0.5f, 0.5f, -0.5}
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
    {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
    {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
    {
        0, 1, 2,
        2, 3, 0,
        4, 5, 6,
        6, 7, 4
    }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);
    return scenegraph;
}

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, const VkExtent2D& extent)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.0001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0),
                                      centre, vsg::dvec3(0.0, 0.0, 1.0));

    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height),
                                                nearFarRatio*radius, radius * 4.5);

    return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "rendertotexture";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"-t", "--test"})) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; windowTraits->fullscreen = true; }
    if (arguments.read({"--st", "--small-test"})) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; windowTraits->width = 192, windowTraits->height = 108; windowTraits->decoration = false; }
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
    auto useDatabasePager = arguments.read("--pager");
    auto maxPageLOD = arguments.value(-1, "--max-plod");
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    bool separateCommandGraph = arguments.read("-s");
    bool multiThreading = arguments.read("--mt");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    VsgNodes vsgNodes;

    vsg::Path path;

    // read any vsg files
    for (int i=1; i<argc; ++i)
    {
        vsg::Path filename = arguments[i];

        path = vsg::filePath(filename);

        auto loaded_scene = vsg::read_cast<vsg::Node>(filename);
        if (loaded_scene)
        {
            vsgNodes.push_back(loaded_scene);
            arguments.remove(i, 1);
            --i;
        }
    }

    // assign the vsg_scene from the loaded nodes
    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (vsgNodes.size()>1)
    {
        auto vsg_group = vsg::Group::create();
        for(auto& subgraphs : vsgNodes)
        {
            vsg_group->addChild(subgraphs);
        }

        vsg_scene = vsg_group;
    }
    else if (vsgNodes.size()==1)
    {
        vsg_scene = vsgNodes.front();
    }

    if (!vsg_scene)
    {
        std::cout<<"No command graph created."<<std::endl;
        return 1;
    }

    // A hack for getting the example teapot into the correct orientation
    auto zUp = vsg::MatrixTransform::create(vsg::dmat4(1.0, 0.0, 0.0, 0.0,
                                                       0.0, 0.0, -1.0, 0.0,
                                                       0.0, 1.0, 0.0, 0.0,
                                                       0.0, 0.0, 0.0, 1.0));
    zUp->addChild(vsg_scene);

    // Transform for rotation animation
    auto transform = vsg::MatrixTransform::create();
    transform->addChild(zUp);
    vsg_scene = transform;
    
    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // for convenience create a compile context for creating our
    // storage image
    // XXX How to put ImageData in the scene graph and compile it
    // during a compile traversal? Should RenderGraph support compile()?
    vsg::CompileTraversal compile(window);

    // Framebuffer with attachments
    VkExtent2D targetExtent{512, 512};
    auto offscreenCamera = createCameraForScene(vsg_scene, targetExtent);
    vsg::ImageData colorImage, depthImage;
    auto sceneRenderGraph = createOffscreenRendergraph(window->getOrCreateDevice(), compile.context, offscreenCamera, targetExtent,
                                                       colorImage, depthImage);
    sceneRenderGraph->addChild(vsg_scene);
    // Planes geometry that uses the rendered scene as a texture map
    vsg::ref_ptr<vsg::Node> planes = createPlanes(colorImage);
    auto camera = createCameraForScene(planes, window->extent2D());

    // set up database pager
    vsg::ref_ptr<vsg::DatabasePager> databasePager;
    if (useDatabasePager)
    {
        databasePager = vsg::DatabasePager::create();
        if (maxPageLOD>=0) databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPageLOD;
    }

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    if (separateCommandGraph)
    {
        std::cout<<"Using separateCommandGraph"<<std::endl;
        auto renderToTexture_commandGraph = vsg::CommandGraph::create(window);
        renderToTexture_commandGraph->addChild(sceneRenderGraph);

        auto main_commandGraph = vsg::createCommandGraphForView(window, camera, planes);

        viewer->assignRecordAndSubmitTaskAndPresentation({renderToTexture_commandGraph, main_commandGraph}, databasePager);
    }
    else
    {
        auto commandGraph = vsg::createCommandGraphForView(window, camera, planes);
        // Place the offscreen RenderGraph before the plane geometry RenderGraph
        commandGraph->getChildren().emplace(commandGraph->getChildren().begin(), sceneRenderGraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph}, databasePager);
    }

    viewer->compile();

    if (multiThreading)
    {
        viewer->setupThreading();
    }

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        // animate the offscreen scenegraph
        float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
        transform->setMatrix(vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f)));

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
