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

#include <cassert>
#include <chrono>
#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>

bool supportsBlit(vsg::ref_ptr<vsg::Device> device, VkFormat format)
{
    auto physicalDevice = device->getPhysicalDevice();
    VkFormatProperties srcFormatProperties;
    vkGetPhysicalDeviceFormatProperties(*(physicalDevice), format, &srcFormatProperties);
    VkFormatProperties destFormatProperties;
    vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);
    return ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
           ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);
}

vsg::ref_ptr<vsg::Image> createCaptureImage(
    vsg::ref_ptr<vsg::Device> device,
    VkFormat sourceFormat,
    const VkExtent2D& extent)
{
    // blit to RGBA if supported
    auto targetFormat = supportsBlit(device, sourceFormat) ? VK_FORMAT_R8G8B8A8_UNORM : sourceFormat;

    // create image to write to
    auto image = vsg::Image::create();
    image->format = targetFormat;
    image->extent = {extent.width, extent.height, 1};
    image->arrayLayers = 1;
    image->mipLevels = 1;
    image->tiling = VK_IMAGE_TILING_LINEAR;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    image->compile(device);

    auto memReqs = image->getMemoryRequirements(device->deviceID);
    auto memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    auto deviceMemory = vsg::DeviceMemory::create(device, memReqs, memFlags);
    image->bind(deviceMemory, 0);

    return image;
}

VkImageUsageFlags computeUsageFlagsForFormat(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        default:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
}

vsg::ref_ptr<vsg::ImageView> createTransferImageView(
    vsg::ref_ptr<vsg::Device> device,
    VkFormat format,
    const VkExtent2D& extent,
    VkSampleCountFlagBits samples)
{
    auto image = vsg::Image::create();
    image->format = format;
    image->extent = VkExtent3D{extent.width, extent.height, 1};
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->samples = samples;
    image->usage = computeUsageFlagsForFormat(format) | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    return vsg::createImageView(device, image, vsg::computeAspectFlagsForFormat(format));
}

vsg::ref_ptr<vsg::Commands> createTransferCommands(
    vsg::ref_ptr<vsg::Image> sourceImage,
    vsg::ref_ptr<vsg::Image> destinationImage,
    vsg::ref_ptr<vsg::Commands> commands = {})
{
    // create or reset the commands
    if (!commands)
    {
        commands = vsg::Commands::create();
    }
    else
    {
        commands->children.clear();
    }

    // transition destinationImage to transfer destination initialLayout
    auto transitionDestinationImageToDestinationLayoutBarrier = vsg::ImageMemoryBarrier::create(
        0,                                                             // srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,                                  // dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,                                     // oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // newLayout
        VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
        destinationImage,                                              // image
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
    );

    auto cmd_transitionForTransferBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT,                       // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,                       // dstStageMask
        0,                                                    // dependencyFlags
        transitionDestinationImageToDestinationLayoutBarrier  // barrier
    );

    commands->addChild(cmd_transitionForTransferBarrier);

    if (sourceImage->format == destinationImage->format)
    {
        // use vkCmdCopyImage
        VkImageCopy region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.extent = destinationImage->extent;

        auto copyImage = vsg::CopyImage::create();
        copyImage->srcImage = sourceImage;
        copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyImage->dstImage = destinationImage;
        copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyImage->regions.push_back(region);

        commands->addChild(copyImage);
    }
    else
    {
        // blit using vkCmdBlitImage
        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0] = VkOffset3D{0, 0, 0};
        region.srcOffsets[1] = VkOffset3D{
            static_cast<int32_t>(sourceImage->extent.width),
            static_cast<int32_t>(sourceImage->extent.height),
            1};
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0] = VkOffset3D{0, 0, 0};
        region.dstOffsets[1] = VkOffset3D{
            static_cast<int32_t>(destinationImage->extent.width),
            static_cast<int32_t>(destinationImage->extent.height),
            1};

        auto blitImage = vsg::BlitImage::create();
        blitImage->srcImage = sourceImage;
        blitImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitImage->dstImage = destinationImage;
        blitImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitImage->regions.push_back(region);
        blitImage->filter = VK_FILTER_NEAREST;

        commands->addChild(blitImage);
    }

    // transition destination image from transfer destination layout
    // to general layout to enable mapping to image DeviceMemory
    auto transitionDestinationImageToMemoryReadBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_WRITE_BIT,                                  // srcAccessMask
        VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // oldLayout
        VK_IMAGE_LAYOUT_GENERAL,                                       // newLayout
        VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
        destinationImage,                                              // image
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
    );

    auto cmd_transitionFromTransferBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT,                // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
        0,                                             // dependencyFlags
        transitionDestinationImageToMemoryReadBarrier  // barrier
    );

    commands->addChild(cmd_transitionFromTransferBarrier);

    return commands;
}

vsg::ref_ptr<vsg::RenderPass> createTransferRenderPass(
    vsg::ref_ptr<vsg::Device> device,
    VkFormat imageFormat,
    VkFormat depthFormat,
    bool requiresDepthRead)
{
    auto colorAttachment = vsg::defaultColorAttachment(imageFormat);
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // <-- difference from vsg::createRenderPass()

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

vsg::ref_ptr<vsg::RenderPass> createTransferRenderPass(
    vsg::ref_ptr<vsg::Device> device,
    VkFormat imageFormat,
    VkFormat depthFormat,
    VkSampleCountFlagBits samples,
    bool requiresDepthRead)
{
    vsg::ref_ptr<vsg::RenderPass> renderPass;

    if (samples == VK_SAMPLE_COUNT_1_BIT)
    {
        return createTransferRenderPass(device, imageFormat, depthFormat, requiresDepthRead);
    }

    // First attachment is supersampled or multisampled target.
    vsg::AttachmentDescription colorAttachment = {};
    colorAttachment.format = imageFormat;
    colorAttachment.samples = samples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Second attachment is the resolved image which will be presented.
    vsg::AttachmentDescription resolveAttachment = {};
    resolveAttachment.format = imageFormat;
    resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // <-- difference from vsg::createMultisampledRenderPass()

    // supersampled or multisampled depth attachment. Resolved if requiresDepthRead is true.
    vsg::AttachmentDescription depthAttachment = {};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = samples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::RenderPass::Attachments attachments{colorAttachment, resolveAttachment, depthAttachment};

    if (requiresDepthRead)
    {
        vsg::AttachmentDescription depthResolveAttachment = {};
        depthResolveAttachment.format = depthFormat;
        depthResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthResolveAttachment);
    }

    vsg::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::AttachmentReference resolveAttachmentRef = {};
    resolveAttachmentRef.attachment = 1;
    resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::AttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 2;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::SubpassDescription subpass;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachments.emplace_back(colorAttachmentRef);
    subpass.resolveAttachments.emplace_back(resolveAttachmentRef);
    subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

    if (requiresDepthRead)
    {
        vsg::AttachmentReference depthResolveAttachmentRef = {};
        depthResolveAttachmentRef.attachment = 3;
        depthResolveAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass.depthResolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        subpass.stencilResolveMode = VK_RESOLVE_MODE_NONE;
        subpass.depthStencilResolveAttachments.emplace_back(depthResolveAttachmentRef);
    }

    vsg::RenderPass::Subpasses subpasses{subpass};

    vsg::SubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    vsg::SubpassDependency dependency2 = {};
    dependency2.srcSubpass = 0;
    dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency2.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependency2.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependency2.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    vsg::RenderPass::Dependencies dependencies{dependency, dependency2};

    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}

vsg::ref_ptr<vsg::Framebuffer> createOffscreenFramebuffer(
    vsg::ref_ptr<vsg::Device> device,
    vsg::ref_ptr<vsg::ImageView> transferImageView,
    VkSampleCountFlagBits const samples)
{
    constexpr VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    constexpr bool requiresDepthRead = false;

    VkExtent2D const extent{
        transferImageView->image->extent.width,
        transferImageView->image->extent.height
    };

    vsg::ImageViews imageViews;
    if (samples == VK_SAMPLE_COUNT_1_BIT)
    {
        imageViews.emplace_back(transferImageView);
        imageViews.emplace_back(createTransferImageView(device, depthFormat, extent, VK_SAMPLE_COUNT_1_BIT));
    }
    else
    {
        // MSAA
        imageViews.emplace_back(createTransferImageView(device, imageFormat, extent, samples));
        imageViews.emplace_back(transferImageView);
        imageViews.emplace_back(createTransferImageView(device, depthFormat, extent, samples));
        if (requiresDepthRead)
        {
            imageViews.emplace_back(createTransferImageView(device, depthFormat, extent, VK_SAMPLE_COUNT_1_BIT));
        }
    }

    auto renderPass = createTransferRenderPass(device, imageFormat, depthFormat, samples, requiresDepthRead);
    auto framebuffer = vsg::Framebuffer::create(renderPass, imageViews, extent.width, extent.height, 1);

    return framebuffer;
}

vsg::ref_ptr<vsg::Data> getImageData(vsg::ref_ptr<vsg::Device> device, vsg::ref_ptr<vsg::Image> image)
{
    VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(*device, image->vk(device->deviceID), &subResource, &subResourceLayout);

    auto deviceMemory = image->getDeviceMemory(device->deviceID);

    VkExtent2D extent{image->extent.width, image->extent.height};

    size_t destRowWidth = extent.width * sizeof(vsg::ubvec4);
    vsg::ref_ptr<vsg::Data> imageData;
    if (destRowWidth == subResourceLayout.rowPitch)
    {
        // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
        imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(
            deviceMemory,
            subResourceLayout.offset,
            0,
            vsg::Data::Properties{image->format},
            extent.width,
            extent.height);
    }
    else
    {
        // Map the buffer memory and assign as a ubyteArray that will automatically unmap itself on destruction.
        // A ubyteArray is used as the graphics buffer memory is not contiguous like vsg::Array2D, so map to a flat buffer first then copy to Array2D.
        auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(
            deviceMemory,
            subResourceLayout.offset,
            0,
            vsg::Data::Properties{image->format},
            subResourceLayout.rowPitch * extent.height);
        imageData = vsg::ubvec4Array2D::create(extent.width, extent.height, vsg::Data::Properties{image->format});
        for (uint32_t row = 0; row < extent.height; ++row)
        {
            std::memcpy(imageData->dataPointer(row*extent.width), mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
        }
    }
    return imageData;
}

class ScreenshotHandler : public vsg::Inherit<vsg::Visitor, ScreenshotHandler>
{
public:
    bool do_image_capture = false;
    vsg::ref_ptr<vsg::Image> image;
    vsg::Path filename;
    vsg::ref_ptr<vsg::Options> options;

    ScreenshotHandler(
        vsg::ref_ptr<vsg::Image> in_image,
        vsg::Path const& in_filename,
        vsg::ref_ptr<vsg::Options> in_options = {})
    : image(in_image)
    , filename(in_filename)
    , options(in_options)
    {}

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase == 's')
        {
            do_image_capture = true;
        }
    }

    void screenshot_image(vsg::ref_ptr<vsg::Device> device)
    {
        do_image_capture = false;

        vsg::info("writing image to file: ", filename);
        auto imageData = getImageData(device, image);
        vsg::write(imageData, filename, options);
    }
};

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scenegraph, const VkExtent2D& extent)
{
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto eye = centre + vsg::dvec3(0.0, -radius * 3.5, 0.0);
    auto target = centre;
    auto up = vsg::dvec3(0.0, 0.0, 1.0);
    auto lookAt = vsg::LookAt::create(eye, target, up);

    double fieldOfViewY = 30.0;
    double aspectRatio = static_cast<double>(extent.width) / static_cast<double>(extent.height);
    double nearDistance = nearFarRatio * radius;
    double farDistance = radius * 4.5;
    auto perspective = vsg::Perspective::create(fieldOfViewY, aspectRatio, nearDistance, farDistance);

    return vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));
}

void replaceChild(vsg::Group* group, vsg::ref_ptr<vsg::Node> previous, vsg::ref_ptr<vsg::Node> replacement)
{
    bool replaced = false;
    for (auto& child : group->children)
    {
        if (child == previous)
        {
            child = replacement;
            replaced = true;
        }
    }
    assert(replaced);
    vsg::info("commands replaced.");
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
    arguments.read({"--extent"}, windowTraits->width, windowTraits->height);

    bool nestedCommandGraph = arguments.read({"-n", "--nested"});
    bool separateCommandGraph = arguments.read("-s");

    // offscreen capture super and multi sampling parameters
    auto captureFilename = arguments.value<vsg::Path>("screenshot.vsgt", {"--capture-file", "-f"});
    unsigned short supersample = 1;
    arguments.read("--supersample", supersample);
    bool msaa = arguments.read("--msaa");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // if we are multisampling then to enable copying of the depth buffer we have to
    // enable a depth buffer resolve extension for vsg::RenderPass or require a minimum vulkan version of 1.2
    if (msaa) windowTraits->vulkanVersion = VK_API_VERSION_1_2;

    // sampling for offscreen rendering
    VkSampleCountFlagBits samples = msaa ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    VsgNodes vsgNodes;

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    // read any vsg files
    for (int i = 1; i < argc; ++i)
    {
        vsg::Path filename = arguments[i];
        auto loaded_scene = vsg::read_cast<vsg::Node>(filename, options);
        if (loaded_scene)
        {
            vsgNodes.push_back(loaded_scene);
            arguments.remove(i, 1);
            --i;
        }
    }

    // assign the vsg_scene from the loaded nodes
    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (vsgNodes.size() > 1)
    {
        auto vsg_group = vsg::Group::create();
        for (auto& subgraphs : vsgNodes)
        {
            vsg_group->addChild(subgraphs);
        }

        vsg_scene = vsg_group;
    }
    else if (vsgNodes.size() == 1)
    {
        vsg_scene = vsgNodes.front();
    }

    if (!vsg_scene)
    {
        std::cout << "No valid model files specified." << std::endl;
        return 1;
    }

    // Transform for rotation animation
    auto transform = vsg::MatrixTransform::create();
    transform->addChild(vsg_scene);
    vsg_scene = transform;

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    auto displayCamera = createCameraForScene(vsg_scene, window->extent2D());
    auto displayRenderGraph = vsg::createRenderGraphForView(window, displayCamera, vsg_scene);

    auto context = vsg::Context::create(window->getOrCreateDevice());

    VkFormat offscreenImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D extent{windowTraits->width, windowTraits->height};
    VkExtent2D offscreenRenderExtent{supersample * extent.width, supersample * extent.height};

    auto transferImageView = createTransferImageView(context->device, offscreenImageFormat, offscreenRenderExtent, VK_SAMPLE_COUNT_1_BIT);
    auto captureImage = createCaptureImage(context->device, offscreenImageFormat, extent);
    auto captureCommands = createTransferCommands(transferImageView->image, captureImage);

    auto offscreenRenderGraph = vsg::RenderGraph::create();
    offscreenRenderGraph->framebuffer = createOffscreenFramebuffer(context->device, transferImageView, samples);
    offscreenRenderGraph->renderArea.extent = offscreenRenderGraph->framebuffer->extent2D();
    offscreenRenderGraph->setClearValues(
        VkClearColorValue{{0.0f, 0.0f, 0.0f, 0.0f}},
        VkClearDepthStencilValue{0.0f, 0});

    // link view matrix between display and offscreen render
    // the projection may be different (aspect ratio for example)
    //auto offscreenCamera = createCameraForScene(vsg_scene, extent);
    //offscreenCamera->viewMatrix = displayCamera->viewMatrix;
    //auto offscreenView = vsg::View::create(offscreenCamera, vsg_scene);
    auto offscreenView = vsg::View::create(displayCamera, vsg_scene);
    offscreenRenderGraph->addChild(offscreenView);

    auto offscreenSwitch = vsg::Switch::create();
    bool offscreenEnabled = false;
    offscreenSwitch->addChild(offscreenEnabled, offscreenRenderGraph);

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(displayCamera));

    auto screenshotHandler = ScreenshotHandler::create(captureImage, captureFilename, options);
    viewer->addEventHandler(screenshotHandler);

    vsg::ref_ptr<vsg::CommandGraph> offscreenCommandGraph;
    if (nestedCommandGraph)
    {
        std::cout<<"Nested CommandGraph, with nested offscreenCommandGraph as a child on the displayCommandGraph. "<<std::endl;
        offscreenCommandGraph = vsg::CommandGraph::create(window);
        offscreenCommandGraph->submitOrder = -1; // render before the displayRenderGraph
        offscreenCommandGraph->addChild(offscreenSwitch);
        offscreenCommandGraph->addChild(captureCommands);

        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(displayRenderGraph);
        commandGraph->addChild(offscreenCommandGraph); // offscreenCommandGraph nested within main CommandGraph

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    }
    else if (separateCommandGraph)
    {
        std::cout<<"Seperate CommandGraph with offscreenCommandGraph first, then main CommandGraph second."<<std::endl;
        offscreenCommandGraph = vsg::CommandGraph::create(window);
        offscreenCommandGraph->submitOrder = -1; // render before the displayCommandGraph
        offscreenCommandGraph->addChild(offscreenSwitch);
        offscreenCommandGraph->addChild(captureCommands);

        auto displayCommandGraph = vsg::CommandGraph::create(window);
        displayCommandGraph->addChild(displayRenderGraph);

        viewer->assignRecordAndSubmitTaskAndPresentation({offscreenCommandGraph, displayCommandGraph});
    }
    else
    {
        std::cout<<"Single CommandGraph containing by the offscreen and main RenderGraphs"<<std::endl;
        auto commandGraph = vsg::CommandGraph::create(window);
        commandGraph->addChild(offscreenSwitch);
        commandGraph->addChild(captureCommands);
        commandGraph->addChild(displayRenderGraph);

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        // offscreenCommandGraph used to handle interactive resize
        offscreenCommandGraph = commandGraph;
    }

    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        // animate the offscreen scenegraph
        float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
        transform->matrix = vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

        if (screenshotHandler->do_image_capture && !offscreenEnabled)
        {
            VkExtent2D displayExtent = displayCamera->getRenderArea().extent;
            if (extent.width != displayExtent.width || extent.height != displayExtent.height)
            {
                vsg::info("display extent changed to ", displayExtent.width, "x", displayExtent.height);
                auto prevCaptureCommands = captureCommands;

                extent = displayExtent;
                offscreenRenderExtent = VkExtent2D{supersample * extent.width, supersample * extent.height};

                transferImageView = createTransferImageView(context->device, offscreenImageFormat, offscreenRenderExtent, VK_SAMPLE_COUNT_1_BIT);
                captureImage = createCaptureImage(context->device, offscreenImageFormat, extent);
                captureCommands = createTransferCommands(transferImageView->image, captureImage);

                offscreenRenderGraph->framebuffer = createOffscreenFramebuffer(context->device, transferImageView, samples);

                replaceChild(offscreenCommandGraph, prevCaptureCommands, captureCommands);

                screenshotHandler->image = captureImage;

                offscreenRenderGraph->resized();
            }

            offscreenEnabled = true;
            offscreenSwitch->setAllChildren(offscreenEnabled);
        }

        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();

        if (screenshotHandler->do_image_capture && offscreenEnabled)
        {
            screenshotHandler->screenshot_image(context->device);
            offscreenEnabled = false;
            offscreenSwitch->setAllChildren(offscreenEnabled);
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
