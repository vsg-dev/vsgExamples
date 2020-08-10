/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield
Copyright(c) 2020 Tim Moore

Portions derived from code that is Copyright (C) Sascha Willems - www.saschawillems.de

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

#include <iostream>
#include <chrono>
#include <thread>

#include "../shared/AnimationPath.h"

vsg::ref_ptr<vsg::ImageView> createColorImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat imageFormat)
{
   VkImageCreateInfo colorImageCreateInfo;
    colorImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    colorImageCreateInfo.format = imageFormat;
    colorImageCreateInfo.extent = VkExtent3D{extent.width, extent.height, 1};
    colorImageCreateInfo.mipLevels = 1;
    colorImageCreateInfo.arrayLayers = 1;
    colorImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    colorImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImageCreateInfo.flags = 0;
    colorImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    colorImageCreateInfo.queueFamilyIndexCount = 0;
    colorImageCreateInfo.pNext = nullptr;
    return vsg::createImageView(device, colorImageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT);
}

VkImageAspectFlags computeAspectFlagsForDepthFormat(VkFormat depthFormat)
{
    if (depthFormat==VK_FORMAT_D24_UNORM_S8_UINT) return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    else return VK_IMAGE_ASPECT_DEPTH_BIT;
}

vsg::ref_ptr<vsg::ImageView> createDepthImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat depthFormat)
{
    VkImageCreateInfo depthImageCreateInfo = {};
    depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.extent = VkExtent3D{extent.width, extent.height, 1};
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.format = depthFormat;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageCreateInfo.flags = 0;
    depthImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageCreateInfo.pNext = nullptr;

    return vsg::createImageView(device, depthImageCreateInfo, computeAspectFlagsForDepthFormat(depthFormat));
}

std::pair<vsg::ref_ptr<vsg::Commands>, vsg::ref_ptr<vsg::Image>> createColorCapture(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, vsg::ref_ptr<vsg::Image> sourceImage, VkFormat sourceImageFormat)
{
    auto width = extent.width;
    auto height = extent.height;

    auto physicalDevice = device->getPhysicalDevice();

    VkFormat targetImageFormat = sourceImageFormat;

    //
    // 1) Check to see of Blit is supported.
    //
    VkFormatProperties srcFormatProperties;
    vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);

    VkFormatProperties destFormatProperties;
    vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_UNORM, &destFormatProperties);

    bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                        ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);

    if (supportsBlit)
    {
        // we can automatically convert the image format when blit, so take advantage of it to ensure RGBA
        targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }

    //std::cout<<"supportsBlit = "<<supportsBlit<<std::endl;

    //
    // 2) create image to write to
    //
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = targetImageFormat;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    auto destinationImage = vsg::Image::create(device, imageCreateInfo);

    auto deviceMemory = vsg::DeviceMemory::create(device, destinationImage->getMemoryRequirements(device->deviceID),  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    destinationImage->bind(deviceMemory, 0);

    //
    // 3) create command buffer and submit to graphcis queue
    //
    auto commands = vsg::Commands::create();

    // 3.a) tranisistion destinationImage to transfer destination initialLayout
    auto transitionDestinationImageToDestinationLayoutBarrier = vsg::ImageMemoryBarrier::create(
        0, // srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED, // oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newLayout
        VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
        destinationImage, // image
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourceRange
    );

    // 3.b) transition swapChainImage from present to transfer source initialLayout
    auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_MEMORY_READ_BIT, // srcAccessMask
        VK_ACCESS_TRANSFER_READ_BIT, // dstAccessMask
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // newLayout
        VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
        sourceImage, // image
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourceRange
    );

    auto cmd_transitionForTransferBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
        0, // dependencyFlags
        transitionDestinationImageToDestinationLayoutBarrier, // barrier
        transitionSourceImageToTransferSourceLayoutBarrier // barrier
    );

    commands->addChild(cmd_transitionForTransferBarrier);

    if (supportsBlit)
    {
        // 3.c.1) if blit using VkCmdBliImage
        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0] = VkOffset3D{0, 0, 0};
        region.srcOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 0};
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0] = VkOffset3D{0, 0, 0};
        region.dstOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 0};

        auto blitImage = vsg::BlitImage::create();
        blitImage->srcImage = sourceImage;
        blitImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitImage->dstImage = destinationImage;
        blitImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitImage->regions.push_back(region);
        blitImage->filter = VK_FILTER_NEAREST;

        commands->addChild(blitImage);
    }
    else
    {
        // 3.c.2) else use VkVmdCopyImage

        VkImageCopy region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.extent.width = width;
        region.extent.height = height;
        region.extent.depth = 1;

        auto copyImage = vsg::CopyImage::create();
        copyImage->srcImage = sourceImage;
        copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyImage->dstImage = destinationImage;
        copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyImage->regions.push_back(region);

        commands->addChild(copyImage);
    }

    // 3.d) tranisition destinate image from transder destination layout to general laytout to enable mapping to image DeviceMemory
    auto transitionDestinationImageToMemoryReadBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
        VK_ACCESS_MEMORY_READ_BIT, // dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldLayout
        VK_IMAGE_LAYOUT_GENERAL, // newLayout
        VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
        destinationImage, // image
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourceRange
    );

    // 3.e) transition sawp chain image back to present
    auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_READ_BIT, // srcAccessMask
        VK_ACCESS_MEMORY_READ_BIT, // dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // oldLayout
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // newLayout
        VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
        sourceImage, // image
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourceRange
    );

    auto cmd_transitionFromTransferBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
        0, // dependencyFlags
        transitionDestinationImageToMemoryReadBarrier, // barrier
        transitionSourceImageBackToPresentBarrier // barrier
    );

    commands->addChild(cmd_transitionFromTransferBarrier);

    return {commands, destinationImage};
}

std::pair<vsg::ref_ptr<vsg::Commands>, vsg::ref_ptr<vsg::Buffer>> createDepthCapture(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, vsg::ref_ptr<vsg::Image> sourceImage, VkFormat sourceImageFormat)
{
    auto width = extent.width;
    auto height = extent.height;

    auto physicalDevice = device->getPhysicalDevice();

    VkFormat targetImageFormat = sourceImageFormat;

    auto memoryRequirements = sourceImage->getMemoryRequirements(device->deviceID);

    // 1. create buffer to copy to.
    VkDeviceSize bufferSize = memoryRequirements.size;
    vsg::ref_ptr<vsg::Buffer> destinationBuffer = vsg::Buffer::create(device, memoryRequirements.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE);
    vsg::ref_ptr<vsg::DeviceMemory> destinationMemory = vsg::DeviceMemory::create(device, destinationBuffer->getMemoryRequirements(), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    destinationBuffer->bind(destinationMemory, 0);

    VkImageAspectFlags imageAspectFlags = computeAspectFlagsForDepthFormat(sourceImageFormat);

    // 2.a) tranition depth image for reading
    auto commands = vsg::Commands::create();

    auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // srcAccessMask
        VK_ACCESS_TRANSFER_READ_BIT, // dstAccessMask
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // newLayout
        VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
        sourceImage, // image
        VkImageSubresourceRange{ imageAspectFlags, 0, 1, 0, 1 } // subresourceRange
    );

    auto transitionDestinationBufferToTransferWriteBarrier = vsg::BufferMemoryBarrier::create(
        VK_ACCESS_MEMORY_READ_BIT, // srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
        destinationBuffer, // buffer
        0, // offset
        bufferSize // size
    );

    auto cmd_transitionSourceImageToTransferSourceLayoutBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
        0, // dependencyFlags
        transitionSourceImageToTransferSourceLayoutBarrier, // barrier
        transitionDestinationBufferToTransferWriteBarrier // barrier
    );
    commands->addChild(cmd_transitionSourceImageToTransferSourceLayoutBarrier);

    // 2.b) copy image to buffer
    {
        VkBufferImageCopy region{};
        region.bufferOffset;
        region.bufferRowLength = width; // need to figure out actual row length from somewhere...
        region.bufferImageHeight = height;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = VkOffset3D{0, 0, 0};
        region.imageExtent = VkExtent3D{width, height, 1};

        auto copyImage = vsg::CopyImageToBuffer::create();
        copyImage->srcImage = sourceImage;
        copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyImage->dstBuffer = destinationBuffer;
        copyImage->regions.push_back(region);

        commands->addChild(copyImage);
    }


    // 2.c) transition dpeth image back for rendering
    auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_READ_BIT, // srcAccessMask
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // oldLayout
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // newLayout
        VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
        sourceImage, // image
        VkImageSubresourceRange{ imageAspectFlags, 0, 1, 0, 1 } // subresourceRange
    );

    auto transitionDestinationBufferToMemoryReadBarrier = vsg::BufferMemoryBarrier::create(
        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
        VK_ACCESS_MEMORY_READ_BIT, // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
        destinationBuffer, // buffer
        0, // offset
        bufferSize // size
    );

    auto cmd_transitionSourceImageBackToPresentBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
        0, // dependencyFlags
        transitionSourceImageBackToPresentBarrier, // barrier
        transitionDestinationBufferToMemoryReadBarrier // barrier
    );

    commands->addChild(cmd_transitionSourceImageBackToPresentBarrier);

    return {commands, destinationBuffer};
}

vsg::ref_ptr<vsg::RenderPass> createRenderPassCompatibleWithReadingDepthBuffer(vsg::Device* device, VkFormat imageFormat, VkFormat depthFormat)
{
    auto colorAttachmet = vsg::defaultColorAttachment(imageFormat);
    auto depthAttachment = vsg::defaultDepthAttachment(depthFormat);

    // by deault storeOp is VK_ATTACHMENT_STORE_OP_DONT_CARE but we do care, so bake sure we store the depth value
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    vsg::RenderPass::Attachments attachments{colorAttachmet, depthAttachment};

    vsg::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachments.emplace_back(VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    subpass.depthStencilAttachments.emplace_back(VkAttachmentReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});

    vsg::RenderPass::Subpasses subpasses{subpass};

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vsg::RenderPass::Dependencies dependencies{dependency};

    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    VkExtent2D extent{1920, 1020};
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    vsg::Path colorFilename("screenshot.vsgb");
    vsg::Path depthFilename("depth.vsgb");

    vsg::CommandLine arguments(&argc, argv);
    arguments.read({"--extent", "-w"}, extent.width, extent.height);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto databasePager = vsg::DatabasePager::create_if( arguments.read("--pager") );
    auto numFrames = arguments.value(100, "-f");
    auto pathFilename = arguments.value(std::string(),"-p");
    if (arguments.read("--st")) extent = VkExtent2D{192,108};
    if (arguments.read("--float")) depthFormat = VK_FORMAT_D32_SFLOAT;

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (argc<=1)
    {
        std::cout<<"Please specify model to load on command line"<<std::endl;
        return 1;
    }

    auto options = vsg::Options::create();
#ifdef USE_VSGXCHANGE
    // add use of vsgXchange's support for reading and writing 3rd party file formats
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif

    auto vsg_scene = vsg::read_cast<vsg::Node>(argv[1], options);
    if (!vsg_scene)
    {
        std::cout<<"No command graph created."<<std::endl;
        return 1;
    }


    // create instance
    vsg::Names instanceExtensions;
    vsg::Names requestedLayers;
    if (debugLayer || apiDumpLayer)
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
        requestedLayers.push_back("VK_LAYER_LUNARG_standard_validation");
        if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    auto instance = vsg::Instance::create(instanceExtensions, validatedNames, nullptr);
    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        std::cout<<"Could not create PhysicalDevice"<<std::endl;
        return 0;
    }

    vsg::Names deviceExtensions;
    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};
    auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, nullptr);


    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.01;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel")); ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio, 0.0);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio*radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));

    // set up the Rendergraph to manage the rendering
    auto colorImageView = createColorImageView(device, extent, imageFormat);
    auto depthImageView = createDepthImageView(device, extent, depthFormat);
    auto renderPass = createRenderPassCompatibleWithReadingDepthBuffer(device, imageFormat, depthFormat);
    auto framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView, depthImageView}, extent.width, extent.height, 1);

    auto renderGraph = vsg::RenderGraph::create();

    renderGraph->framebuffer = framebuffer;
    renderGraph->renderArea.offset = {0, 0};
    renderGraph->renderArea.extent = extent;
    renderGraph->clearValues.resize(2);
    renderGraph->clearValues[0].color = { {0.2f, 0.2f, 0.4f, 1.0f} };
    renderGraph->clearValues[1].depthStencil = VkClearDepthStencilValue{1.0f, 0};

    renderGraph->camera = camera;
    renderGraph->addChild(vsg_scene);


    // create supoort for copying the color buffer
    vsg::ref_ptr<vsg::Image> colorImage;
    auto [colorBufferCapture, copiedColorBuffer] = createColorCapture(device, extent, vsg::ref_ptr<vsg::Image>(colorImageView->getImage()), imageFormat);
    auto [depthBufferCapture, copiedDepthBuffer] = createDepthCapture(device, extent, vsg::ref_ptr<vsg::Image>(depthImageView->getImage()), depthFormat);

    auto commandGraph = vsg::CommandGraph::create(device, queueFamily);
    commandGraph->addChild(renderGraph);
    if (colorBufferCapture) commandGraph->addChild(colorBufferCapture);
    if (depthBufferCapture) commandGraph->addChild(depthBufferCapture);

    // create the viewer
    auto viewer = vsg::Viewer::create();

    if (!pathFilename.empty())
    {
        std::ifstream in(pathFilename);
        if (!in)
        {
            std::cout << "AnimationPat: Could not open animation path file \"" << pathFilename << "\".\n";
            return 1;
        }

        vsg::ref_ptr<vsg::AnimationPath> animationPath(new vsg::AnimationPath);
        animationPath->read(in);

        viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
    }


    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames--)>0)
    {
        std::cout<<"Frame "<<viewer->getFrameStamp()->frameCount<<std::endl;

        // pass any events into EventHandlers assigned to the Viewer, this includes Frame events generated by the viewer each frame
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        if (copiedColorBuffer || copiedDepthBuffer)
        {
            // wait for completion.
            for(auto& recordAndSubmitTask : viewer->recordAndSubmitTasks)
            {
                recordAndSubmitTask->fence()->wait(std::numeric_limits<uint64_t>::max());
            }

            if (copiedColorBuffer)
            {
                VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
                VkSubresourceLayout subResourceLayout;
                vkGetImageSubresourceLayout(*device, copiedColorBuffer->vk(device->deviceID), &subResource, &subResourceLayout);

                auto deviceMemory = copiedColorBuffer->getDeviceMemory(device->deviceID);

                // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
                auto imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Layout{imageFormat}, extent.width, extent.height); // deviceMemory, offset, flags and dimensions

                vsg::write(imageData, colorFilename);
            }

            if (copiedDepthBuffer)
            {
                // 3. map buffer and copy data.
                auto deviceMemory = copiedDepthBuffer->getDeviceMemory();

                // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
                if (depthFormat==VK_FORMAT_D32_SFLOAT || depthFormat==VK_FORMAT_D32_SFLOAT_S8_UINT)
                {
                    auto imageData = vsg::MappedData<vsg::floatArray2D>::create(deviceMemory, 0, 0, vsg::Data::Layout{depthFormat}, extent.width, extent.height); // deviceMemory, offset, flags and dimensions

                    vsg::write(imageData, depthFilename);
                }
                else
                {
                    auto imageData = vsg::MappedData<vsg::uintArray2D>::create(deviceMemory, 0, 0, vsg::Data::Layout{depthFormat}, extent.width, extent.height); // deviceMemory, offset, flags and dimensions

                    vsg::write(imageData, depthFilename);
                }
            }
        }

    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
