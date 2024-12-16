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

vsg::ref_ptr<vsg::ImageView> createColorImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat imageFormat, VkSampleCountFlagBits samples)
{
    auto colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = imageFormat;
    colorImage->extent = VkExtent3D{extent.width, extent.height, 1};
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = samples;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return vsg::createImageView(device, colorImage, VK_IMAGE_ASPECT_COLOR_BIT);
}

vsg::ref_ptr<vsg::ImageView> createDepthImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat depthFormat, VkSampleCountFlagBits samples)
{
    auto depthImage = vsg::Image::create();
    depthImage->imageType = VK_IMAGE_TYPE_2D;
    depthImage->extent = VkExtent3D{extent.width, extent.height, 1};
    depthImage->mipLevels = 1;
    depthImage->arrayLayers = 1;
    depthImage->samples = samples;
    depthImage->format = depthFormat;
    depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImage->flags = 0;
    depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return vsg::createImageView(device, depthImage, vsg::computeAspectFlagsForFormat(depthFormat));
}

std::pair<vsg::ref_ptr<vsg::Commands>, vsg::ref_ptr<vsg::Image>> createColorCapture(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, vsg::ref_ptr<vsg::Image> sourceImage, VkFormat sourceImageFormat)
{
    auto width = extent.width;
    auto height = extent.height;

    auto physicalDevice = device->getPhysicalDevice();

    VkFormat targetImageFormat = sourceImageFormat;

    //
    // 1) Check to see if Blit is supported.
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
    auto destinationImage = vsg::Image::create();
    destinationImage->imageType = VK_IMAGE_TYPE_2D;
    destinationImage->format = targetImageFormat;
    destinationImage->extent.width = width;
    destinationImage->extent.height = height;
    destinationImage->extent.depth = 1;
    destinationImage->arrayLayers = 1;
    destinationImage->mipLevels = 1;
    destinationImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    destinationImage->samples = VK_SAMPLE_COUNT_1_BIT;
    destinationImage->tiling = VK_IMAGE_TILING_LINEAR;
    destinationImage->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    destinationImage->compile(device);

    auto deviceMemory = vsg::DeviceMemory::create(device, destinationImage->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    destinationImage->bind(deviceMemory, 0);

    //
    // 3) create command buffer and submit to graphics queue
    //
    auto commands = vsg::Commands::create();

    // 3.a) transition destinationImage to transfer destination initialLayout
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

    // 3.b) transition swapChainImage from present to transfer source initialLayout
    auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_MEMORY_READ_BIT,                                     // srcAccessMask
        VK_ACCESS_TRANSFER_READ_BIT,                                   // dstAccessMask
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                               // oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // newLayout
        VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
        sourceImage,                                                   // image
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
    );

    auto cmd_transitionForTransferBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT,                       // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,                       // dstStageMask
        0,                                                    // dependencyFlags
        transitionDestinationImageToDestinationLayoutBarrier, // barrier
        transitionSourceImageToTransferSourceLayoutBarrier    // barrier
    );

    commands->addChild(cmd_transitionForTransferBarrier);

    if (supportsBlit)
    {
        // 3.c.1) if blit using vkCmdBlitImage
        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0] = VkOffset3D{0, 0, 0};
        region.srcOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0] = VkOffset3D{0, 0, 0};
        region.dstOffsets[1] = VkOffset3D{static_cast<int32_t>(width), static_cast<int32_t>(height), 1};

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
        // 3.c.2) else use vkCmdCopyImage

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

    // 3.d) transition destination image from transfer destination layout to general layout to enable mapping to image DeviceMemory
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

    // 3.e) transition swap chain image back to present
    auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_READ_BIT,                                   // srcAccessMask
        VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // oldLayout
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                               // newLayout
        VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
        sourceImage,                                                   // image
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // subresourceRange
    );

    auto cmd_transitionFromTransferBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT,                // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
        0,                                             // dependencyFlags
        transitionDestinationImageToMemoryReadBarrier, // barrier
        transitionSourceImageBackToPresentBarrier      // barrier
    );

    commands->addChild(cmd_transitionFromTransferBarrier);

    return {commands, destinationImage};
}

std::pair<vsg::ref_ptr<vsg::Commands>, vsg::ref_ptr<vsg::Buffer>> createDepthCapture(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, vsg::ref_ptr<vsg::Image> sourceImage, VkFormat sourceImageFormat)
{
    auto width = extent.width;
    auto height = extent.height;

    auto memoryRequirements = sourceImage->getMemoryRequirements(device->deviceID);

    // 1. create buffer to copy to.
    VkDeviceSize bufferSize = memoryRequirements.size;
    auto destinationBuffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    VkImageAspectFlags imageAspectFlags = vsg::computeAspectFlagsForFormat(sourceImageFormat);

    // 2.a) transition depth image for reading
    auto commands = vsg::Commands::create();

    auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // srcAccessMask
        VK_ACCESS_TRANSFER_READ_BIT,                                                                // dstAccessMask
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,                                           // oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                                                       // newLayout
        VK_QUEUE_FAMILY_IGNORED,                                                                    // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                                                    // dstQueueFamilyIndex
        sourceImage,                                                                                // image
        VkImageSubresourceRange{imageAspectFlags, 0, 1, 0, 1}                                       // subresourceRange
    );

    auto transitionDestinationBufferToTransferWriteBarrier = vsg::BufferMemoryBarrier::create(
        VK_ACCESS_MEMORY_READ_BIT,    // srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED,      // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,      // dstQueueFamilyIndex
        destinationBuffer,            // buffer
        0,                            // offset
        bufferSize                    // size
    );

    auto cmd_transitionSourceImageToTransferSourceLayoutBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
        VK_PIPELINE_STAGE_TRANSFER_BIT,                                                            // dstStageMask
        0,                                                                                         // dependencyFlags
        transitionSourceImageToTransferSourceLayoutBarrier,                                        // barrier
        transitionDestinationBufferToTransferWriteBarrier                                          // barrier
    );
    commands->addChild(cmd_transitionSourceImageToTransferSourceLayoutBarrier);

    // 2.b) copy image to buffer
    {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
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

    // 2.c) transition depth image back for rendering
    auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_READ_BIT,                                                                // srcAccessMask
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                                                       // oldLayout
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,                                           // newLayout
        VK_QUEUE_FAMILY_IGNORED,                                                                    // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                                                                    // dstQueueFamilyIndex
        sourceImage,                                                                                // image
        VkImageSubresourceRange{imageAspectFlags, 0, 1, 0, 1}                                       // subresourceRange
    );

    auto transitionDestinationBufferToMemoryReadBarrier = vsg::BufferMemoryBarrier::create(
        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
        VK_ACCESS_MEMORY_READ_BIT,    // dstAccessMask
        VK_QUEUE_FAMILY_IGNORED,      // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,      // dstQueueFamilyIndex
        destinationBuffer,            // buffer
        0,                            // offset
        bufferSize                    // size
    );

    auto cmd_transitionSourceImageBackToPresentBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT,                                                            // srcStageMask
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
        0,                                                                                         // dependencyFlags
        transitionSourceImageBackToPresentBarrier,                                                 // barrier
        transitionDestinationBufferToMemoryReadBarrier                                             // barrier
    );

    commands->addChild(cmd_transitionSourceImageBackToPresentBarrier);

    return {commands, destinationBuffer};
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    VkExtent2D extent{2048, 1024};
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    vsg::CommandLine arguments(&argc, argv);
    arguments.read({"--extent", "-w"}, extent.width, extent.height);
    auto debugLayer = arguments.read({"--debug", "-d"});
    auto apiDumpLayer = arguments.read({"--api", "-a"});
    auto databasePager = vsg::DatabasePager::create_if(arguments.read("--pager"));
    auto numFrames = arguments.value(100, "-f");
    auto pathFilename = arguments.value<vsg::Path>("", "-p");
    auto colorFilename = arguments.value<vsg::Path>("screenshot.vsgb", {"--color-file", "--cf"});
    auto depthFilename = arguments.value<vsg::Path>("depth.vsgb", {"--depth-file", "--df"});
    auto resizeCadence = arguments.value(0, "--resize");
    auto useExecuteCommands = arguments.read("--use-ec");
    if (arguments.read("--st")) extent = VkExtent2D{192, 108};
    bool above = arguments.read("--above");
    bool enableGeometryShader = arguments.read("--gs");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (argc <= 1)
    {
        std::cout << "Please specify model to load on command line" << std::endl;
        return 1;
    }

    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    if (arguments.read("--msaa")) samples = VK_SAMPLE_COUNT_8_BIT;
    bool useDepthBuffer = !arguments.read({"--no-depth", "--nd"});

    // if we are multisampling then to enable copying of the depth buffer we have to enable a depth buffer resolve extension for vsg::RenderPass or require a minimum vulkan version of 1.2
    uint32_t vulkanVersion = VK_API_VERSION_1_0;
    if (samples != VK_SAMPLE_COUNT_1_BIT) vulkanVersion = VK_API_VERSION_1_2;

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto vsg_scene = vsg::read_cast<vsg::Node>(argv[1], options);
    if (!vsg_scene)
    {
        std::cout << "No command graph created." << std::endl;
        return 1;
    }

    // create instance
    vsg::Names instanceExtensions;
    vsg::Names requestedLayers;
    if (debugLayer || apiDumpLayer)
    {
        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
        if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    auto instance = vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion);
    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        std::cout << "Could not create PhysicalDevice" << std::endl;
        return 0;
    }

    vsg::Names deviceExtensions;
    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};

    auto deviceFeatures = vsg::DeviceFeatures::create();
    deviceFeatures->get().samplerAnisotropy = VK_TRUE;
    deviceFeatures->get().geometryShader = enableGeometryShader;

    auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = (above) ? vsg::LookAt::create(centre + vsg::dvec3(0.0, 0.0, radius * 1.5), centre, vsg::dvec3(0.0, 1.0, 0.0)) : vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 1.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel"))
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio, 0.0);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(extent.width) / static_cast<double>(extent.height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));

    vsg::ref_ptr<vsg::Framebuffer> framebuffer;
    vsg::ref_ptr<vsg::ImageView> colorImageView;
    vsg::ref_ptr<vsg::ImageView> depthImageView;
    vsg::ref_ptr<vsg::Commands> colorBufferCapture;
    vsg::ref_ptr<vsg::Image> copiedColorBuffer;
    vsg::ref_ptr<vsg::Commands> depthBufferCapture;
    vsg::ref_ptr<vsg::Buffer> copiedDepthBuffer;

    // set up the RenderGraph to manage the rendering
    if (useDepthBuffer)
    {
        colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);
        depthImageView = createDepthImageView(device, extent, depthFormat, VK_SAMPLE_COUNT_1_BIT);
        if (samples == VK_SAMPLE_COUNT_1_BIT)
        {
            auto renderPass = vsg::createRenderPass(device, imageFormat, depthFormat, true);
            framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView, depthImageView}, extent.width, extent.height, 1);
        }
        else
        {
            auto msaa_colorImageView = createColorImageView(device, extent, imageFormat, samples);
            auto msaa_depthImageView = createDepthImageView(device, extent, depthFormat, samples);

            auto renderPass = vsg::createMultisampledRenderPass(device, imageFormat, depthFormat, samples, true);
            framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{msaa_colorImageView, colorImageView, msaa_depthImageView, depthImageView}, extent.width, extent.height, 1);
        }

        // create support for copying the color buffer
        std::tie(colorBufferCapture, copiedColorBuffer) = createColorCapture(device, extent, colorImageView->image, imageFormat);
        std::tie(depthBufferCapture, copiedDepthBuffer) = createDepthCapture(device, extent, depthImageView->image, depthFormat);
    }
    else
    {
        colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);
        if (samples == VK_SAMPLE_COUNT_1_BIT)
        {
            auto renderPass = vsg::createRenderPass(device, imageFormat);
            framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView}, extent.width, extent.height, 1);
        }
        else
        {
            auto msaa_colorImageView = createColorImageView(device, extent, imageFormat, samples);

            auto renderPass = vsg::createMultisampledRenderPass(device, imageFormat, samples);
            framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{msaa_colorImageView, colorImageView}, extent.width, extent.height, 1);
        }

        // create support for copying the color buffer
        std::tie(colorBufferCapture, copiedColorBuffer) = createColorCapture(device, extent, colorImageView->image, imageFormat);
    }

    auto renderGraph = vsg::RenderGraph::create();

    renderGraph->framebuffer = framebuffer;
    renderGraph->renderArea.offset = {0, 0};
    renderGraph->renderArea.extent = extent;
    renderGraph->setClearValues({{1.0f, 1.0f, 0.0f, 0.0f}}, VkClearDepthStencilValue{0.0f, 0});

    auto view = vsg::View::create(camera, vsg_scene);

    vsg::CommandGraphs commandGraphs;
    if (useExecuteCommands)
    {
        auto secondaryCommandGraph = vsg::SecondaryCommandGraph::create(device, queueFamily);
        secondaryCommandGraph->addChild(view);
        secondaryCommandGraph->framebuffer = framebuffer;
        commandGraphs.push_back(secondaryCommandGraph);

        auto executeCommands = vsg::ExecuteCommands::create();
        executeCommands->connect(secondaryCommandGraph);

        renderGraph->contents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
        renderGraph->addChild(executeCommands);
    }
    else
        renderGraph->addChild(view);

    auto commandGraph = vsg::CommandGraph::create(device, queueFamily);
    commandGraph->addChild(renderGraph);
    commandGraphs.push_back(commandGraph);
    if (colorBufferCapture) commandGraph->addChild(colorBufferCapture);
    if (depthBufferCapture) commandGraph->addChild(depthBufferCapture);

    // create the viewer
    auto viewer = vsg::Viewer::create();

    if (pathFilename)
    {
        auto cameraAnimation = vsg::CameraAnimationHandler::create(camera, pathFilename, options);
        viewer->addEventHandler(cameraAnimation);
        if (cameraAnimation->animation) cameraAnimation->play();
    }

    viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs);

    viewer->compile();

    uint64_t waitTimeout = 1999999999; // 1second in nanoseconds.

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames--) > 0)
    {
        std::cout << "Frame " << viewer->getFrameStamp()->frameCount << std::endl;
        if (resizeCadence && (viewer->getFrameStamp()->frameCount > 0) && ((viewer->getFrameStamp()->frameCount) % resizeCadence == 0))
        {
            viewer->deviceWaitIdle();

            extent.width /= 2;
            extent.height /= 2;

            if (extent.width < 1) extent.width = 1;
            if (extent.height < 1) extent.height = 1;

            std::cout << "Resized to " << extent.width << ", " << extent.height << std::endl;

            auto replace_child = [](vsg::Group* group, vsg::ref_ptr<vsg::Node> previous, vsg::ref_ptr<vsg::Node> replacement) {
                for (auto& child : group->children)
                {
                    if (child == previous) child = replacement;
                }
            };

            auto previous_colorBufferCapture = colorBufferCapture;
            auto previous_depthBufferCapture = depthBufferCapture;

            if (useDepthBuffer)
            {
                colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);
                depthImageView = createDepthImageView(device, extent, depthFormat, VK_SAMPLE_COUNT_1_BIT);
                if (samples == VK_SAMPLE_COUNT_1_BIT)
                {
                    auto renderPass = vsg::createRenderPass(device, imageFormat, depthFormat, true);
                    framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView, depthImageView}, extent.width, extent.height, 1);
                }
                else
                {
                    auto msaa_colorImageView = createColorImageView(device, extent, imageFormat, samples);
                    auto msaa_depthImageView = createDepthImageView(device, extent, depthFormat, samples);

                    auto renderPass = vsg::createMultisampledRenderPass(device, imageFormat, depthFormat, samples, true);
                    framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{msaa_colorImageView, colorImageView, msaa_depthImageView, depthImageView}, extent.width, extent.height, 1);
                }

                renderGraph->framebuffer = framebuffer;

                // create new copy subgraphs
                std::tie(colorBufferCapture, copiedColorBuffer) = createColorCapture(device, extent, colorImageView->image, imageFormat);
                std::tie(depthBufferCapture, copiedDepthBuffer) = createDepthCapture(device, extent, depthImageView->image, depthFormat);

                replace_child(commandGraph, previous_colorBufferCapture, colorBufferCapture);
                replace_child(commandGraph, previous_depthBufferCapture, depthBufferCapture);
            }
            else
            {
                colorImageView = createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT);
                if (samples == VK_SAMPLE_COUNT_1_BIT)
                {
                    auto renderPass = vsg::createRenderPass(device, imageFormat);
                    framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView}, extent.width, extent.height, 1);
                }
                else
                {
                    auto msaa_colorImageView = createColorImageView(device, extent, imageFormat, samples);

                    auto renderPass = vsg::createMultisampledRenderPass(device, imageFormat, samples);
                    framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{msaa_colorImageView, colorImageView}, extent.width, extent.height, 1);
                }

                renderGraph->framebuffer = framebuffer;

                // create new copy subgraphs
                std::tie(colorBufferCapture, copiedColorBuffer) = createColorCapture(device, extent, colorImageView->image, imageFormat);

                replace_child(commandGraph, previous_colorBufferCapture, colorBufferCapture);
                replace_child(commandGraph, previous_depthBufferCapture, depthBufferCapture);
            }
        }

        // pass any events into EventHandlers assigned to the Viewer, this includes Frame events generated by the viewer each frame
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        if (copiedColorBuffer || copiedDepthBuffer)
        {
            // wait for completion.
            viewer->waitForFences(0, waitTimeout);

            if (copiedColorBuffer)
            {
                VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
                VkSubresourceLayout subResourceLayout;
                vkGetImageSubresourceLayout(*device, copiedColorBuffer->vk(device->deviceID), &subResource, &subResourceLayout);

                auto deviceMemory = copiedColorBuffer->getDeviceMemory(device->deviceID);

                size_t destRowWidth = extent.width * sizeof(vsg::ubvec4);
                vsg::ref_ptr<vsg::Data> imageData;
                if (destRowWidth == subResourceLayout.rowPitch)
                {
                    // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
                    imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{imageFormat}, extent.width, extent.height);
                }
                else
                {
                    // Map the buffer memory and assign as a ubyteArray that will automatically unmap itself on destruction.
                    // A ubyteArray is used as the graphics buffer memory is not contiguous like vsg::Array2D, so map to a flat buffer first then copy to Array2D.
                    auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{imageFormat}, subResourceLayout.rowPitch * extent.height);
                    imageData = vsg::ubvec4Array2D::create(extent.width, extent.height, vsg::Data::Properties{imageFormat});
                    for (uint32_t row = 0; row < extent.height; ++row)
                    {
                        std::memcpy(imageData->dataPointer(row * extent.width), mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
                    }
                }

                vsg::write(imageData, colorFilename);
            }

            if (copiedDepthBuffer)
            {
                // 3. map buffer and copy data.
                auto deviceMemory = copiedDepthBuffer->getDeviceMemory(device->deviceID);

                // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
                if (depthFormat == VK_FORMAT_D32_SFLOAT || depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
                {
                    auto imageData = vsg::MappedData<vsg::floatArray2D>::create(deviceMemory, 0, 0, vsg::Data::Properties{depthFormat}, extent.width, extent.height); // deviceMemory, offset, flags and dimensions

                    vsg::write(imageData, depthFilename);
                }
                else
                {
                    auto imageData = vsg::MappedData<vsg::uintArray2D>::create(deviceMemory, 0, 0, vsg::Data::Properties{depthFormat}, extent.width, extent.height); // deviceMemory, offset, flags and dimensions

                    vsg::write(imageData, depthFilename);
                }
            }
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
