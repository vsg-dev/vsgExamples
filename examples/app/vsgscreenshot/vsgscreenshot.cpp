#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

class ScreenshotHandler : public vsg::Inherit<vsg::Visitor, ScreenshotHandler>
{
public:
    bool do_image_capture = false;
    bool do_depth_capture = false;
    vsg::ref_ptr<vsg::Event> event;
    bool eventDebugTest = false;
    vsg::Path colorFilename;
    vsg::Path depthFilename;
    vsg::ref_ptr<vsg::Options> options;

    ScreenshotHandler(vsg::ref_ptr<vsg::Event> in_event, const vsg::Path& in_colorFilename, const vsg::Path& in_depthFilename, vsg::ref_ptr<vsg::Options> in_options = {}):
        event(in_event),
        colorFilename(in_colorFilename),
        depthFilename(in_depthFilename),
        options(in_options)
    {
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase == 's')
        {
            do_image_capture = true;
        }
        if (keyPress.keyBase == 'd')
        {
            do_depth_capture = true;
        }
    }

    void printInfo(vsg::ref_ptr<vsg::Window> window)
    {
        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain();
        std::cout << "\nNeed to take screenshot " << window << std::endl;
        std::cout << "    device = " << device << std::endl;
        std::cout << "    physicalDevice = " << physicalDevice << std::endl;
        std::cout << "    swapchain = " << swapchain << std::endl;
        std::cout << "        swapchain->getImageFormat() = " << swapchain->getImageFormat() << std::endl;
        std::cout << "        swapchain->getExtent() = " << swapchain->getExtent().width << ", " << swapchain->getExtent().height << std::endl;

        for (auto& imageView : swapchain->getImageViews())
        {
            std::cout << "        imageview = " << imageView << std::endl;
        }

        std::cout << "    numFrames() = " << window->numFrames() << std::endl;
        for (size_t i = 0; i < window->numFrames(); ++i)
        {
            std::cout << "        imageview[" << i << "] = " << window->imageView(i) << std::endl;
            std::cout << "        framebuffer[" << i << "] = " << window->framebuffer(i) << std::endl;
        }

        std::cout << "    surfaceFormat() = " << window->surfaceFormat().format << ", " << window->surfaceFormat().colorSpace << std::endl;
        std::cout << "    depthFormat() = " << window->depthFormat() << std::endl;
    }

    void screenshot_image(vsg::ref_ptr<vsg::Window> window)
    {
        // printInfo(window);

        do_image_capture = false;

        if (eventDebugTest && event && event->status() == VK_EVENT_RESET)
        {
            std::cout << "event->status() == VK_EVENT_RESET" << std::endl;
            // manually wait for the event to be signaled
            while (event->status() == VK_EVENT_RESET)
            {
                std::cout << "w";
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::cout << std::endl;
        }

        auto width = window->extent2D().width;
        auto height = window->extent2D().height;

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain();

        // get the colour buffer image of the previous rendered frame as the current frame hasn't been rendered yet.  The 1 in window->imageIndex(1) means image from 1 frame ago.
        auto sourceImage = window->imageView(window->imageIndex(1))->image;

        VkFormat sourceImageFormat = swapchain->getImageFormat();
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

        vsg::info("supportsBlit = ", supportsBlit);

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

        if (event)
        {
            vsg::info("Using vsg::Event/vkEvent");
            commands->addChild(vsg::WaitEvents::create(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, event));
            commands->addChild(vsg::ResetEvent::create(event, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));
        }

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
            // 3.c.1) if blit using VkCmdBliImage
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

        // 3.d) transition destinate image from transfer destination layout to general layout to enable mapping to image DeviceMemory
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

        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });

        //
        // 4) map image and copy
        //
        VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(*device, destinationImage->vk(device->deviceID), &subResource, &subResourceLayout);

        size_t destRowWidth = width * sizeof(vsg::ubvec4);
        vsg::ref_ptr<vsg::Data> imageData;
        if (destRowWidth == subResourceLayout.rowPitch)
        {
            imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{targetImageFormat}, width, height); // deviceMemory, offset, flags and dimensions
        }
        else
        {
            // Map the buffer memory and assign as a ubyteArray that will automatically unmap itself on destruction.
            // A ubyteArray is used as the graphics buffer memory is not contiguous like vsg::Array2D, so map to a flat buffer first then copy to Array2D.
            auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{targetImageFormat}, subResourceLayout.rowPitch*height);
            imageData = vsg::ubvec4Array2D::create(width, height, vsg::Data::Properties{targetImageFormat});
            for (uint32_t row = 0; row < height; ++row)
            {
                std::memcpy(imageData->dataPointer(row*width), mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
            }
        }

        if (vsg::write(imageData, colorFilename, options))
        {
            std::cout<<"Written color buffer to "<<colorFilename<<std::endl;
        }
        else
        {
            std::cout<<"Failed to written color buffer to "<<colorFilename<<std::endl;
        }
    }

    void screenshot_depth(vsg::ref_ptr<vsg::Window> window)
    {
        do_depth_capture = false;

        //printInfo(window);
        if (eventDebugTest && event && event->status() == VK_EVENT_RESET)
        {
            std::cout << "event->status() == VK_EVENT_RESET" << std::endl;
            // manually wait for the event to be signaled
            while (event->status() == VK_EVENT_RESET)
            {
                std::cout << "w";
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::cout << std::endl;
        }

        auto width = window->extent2D().width;
        auto height = window->extent2D().height;

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();

        vsg::ref_ptr<vsg::Image> sourceImage(window->getDepthImage());

        VkFormat sourceImageFormat = window->depthFormat();
        VkFormat targetImageFormat = sourceImageFormat;

        auto memoryRequirements = sourceImage->getMemoryRequirements(device->deviceID);

        // 1. create buffer to copy to.
        VkDeviceSize bufferSize = memoryRequirements.size;
        auto destinationBuffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        auto destinationMemory = destinationBuffer->getDeviceMemory(device->deviceID);

        VkImageAspectFlags imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT; // | VK_IMAGE_ASPECT_STENCIL_BIT; // need to match imageAspectFlags setting to WindowTraits::depthFormat.

        // 2.a) transition depth image for reading
        auto commands = vsg::Commands::create();

        if (event)
        {
            commands->addChild(vsg::WaitEvents::create(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, event));
            commands->addChild(vsg::ResetEvent::create(event, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));
        }

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

        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
            commands->record(commandBuffer);
        });

        // 3. map buffer and copy data.

        // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
        if (targetImageFormat == VK_FORMAT_D32_SFLOAT || targetImageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
        {
            auto imageData = vsg::MappedData<vsg::floatArray2D>::create(destinationMemory, 0, 0, vsg::Data::Properties{targetImageFormat}, width, height); // deviceMemory, offset, flags and dimensions

            size_t num_set_depth = 0;
            size_t num_unset_depth = 0;
            for (auto& value : *imageData)
            {
                if (value == 0.0f)
                    ++num_unset_depth;
                else
                    ++num_set_depth;
            }

            std::cout << "num_unset_depth = " << num_unset_depth << std::endl;
            std::cout << "num_set_depth = " << num_set_depth << std::endl;

            if (vsg::write(imageData, depthFilename, options))
            {
                std::cout<<"Written depth buffer to "<<depthFilename<<std::endl;
            }
        }
        else
        {
            auto imageData = vsg::MappedData<vsg::uintArray2D>::create(destinationMemory, 0, 0, vsg::Data::Properties{targetImageFormat}, width, height); // deviceMemory, offset, flags and dimensions

            if (vsg::write(imageData, depthFilename))
            {
                std::cout<<"Written depth buffer to "<<depthFilename<<std::endl;
            }
        }
    }
};

// http://www.mamoniem.com/trick-vulkan-api-with-the-vulkan-api-mirages-multisampled-depth/

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgscreenshot";

    // enable transfer from the colour and depth buffer images
    windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    windowTraits->depthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    arguments.read(options);
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    arguments.read("--samples", windowTraits->samples);
    auto colorFilename = arguments.value<vsg::Path>("screenshot.vsgt", {"--color-file", "--cf"});
    auto depthFilename = arguments.value<vsg::Path>("depth.vsgt", {"--depth-file", "--df"});
    auto use_vkEvent = arguments.read("--use-vkEvent");
    if (arguments.read("--msaa")) windowTraits->samples = VK_SAMPLE_COUNT_8_BIT;
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read("--float")) windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;
    auto numFrames = arguments.value(-1, "-f");

    // if we are multisampling then to enable copying of the depth buffer we have to enable a depth buffer resolve extensions in vsg::RenderPass that requires a minim vulkan version of 1.2
    if (windowTraits->samples != VK_SAMPLE_COUNT_1_BIT) windowTraits->vulkanVersion = VK_API_VERSION_1_2;

    uint32_t vk_major = 1, vk_minor = 0;
    if (std::string vk_version; arguments.read("--vulkan", vk_version))
    {
        char c;
        std::stringstream vk_version_str(vk_version);
        vk_version_str >> vk_major >> c >> vk_minor;
        std::cout<<"vk_major = "<<vk_major<<std::endl;
        std::cout<<"vk_minor = "<<vk_minor<<std::endl;
    }

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    auto vsg_scene = vsg::Group::create();
    for (int i = 1; i < argc; ++i)
    {
        if (auto node = vsg::read_cast<vsg::Node>(arguments[i], options))
        {
            vsg_scene->addChild(node);
        }
    }

    if (vsg_scene->children.empty())
    {
        std::cout << "Please specify a 3d model file on the command line." << std::endl;
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

    windowTraits->deviceExtensionNames = {
        VK_KHR_MULTIVIEW_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME};

    auto device = window->getOrCreateDevice();

    viewer->addWindow(window);

    // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceDepthStencilResolveProperties.html
    auto depthStencilResolve_properites = window->getOrCreatePhysicalDevice()->getProperties<VkPhysicalDeviceDepthStencilResolveProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES>();
    std::cout << "depthStencilResolve_properites.supportedDepthResolveModes = " << depthStencilResolve_properites.supportedDepthResolveModes << std::endl;
    std::cout << "depthStencilResolve_properites.supportedStencilResolveModes = " << depthStencilResolve_properites.supportedStencilResolveModes << std::endl;
    std::cout << "depthStencilResolve_properites.independentResolveNone = " << depthStencilResolve_properites.independentResolveNone << std::endl;
    std::cout << "depthStencilResolve_properites.independentResolve = " << depthStencilResolve_properites.independentResolve << std::endl;

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.01;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (auto ellipsoidModel = vsg_scene->getRefObject<vsg::EllipsoidModel>("EllipsoidModel"))
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, 0.0);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto event = vsg::Event::create_if(use_vkEvent, window->getOrCreateDevice()); // Vulkan creates vkEvent in an unsignalled state

    // Add ScreenshotHandler to respond to keyboard and mouse events.
    auto screenshotHandler = ScreenshotHandler::create(event, colorFilename, depthFilename, options);
    viewer->addEventHandler(screenshotHandler);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    if (event) commandGraph->addChild(vsg::SetEvent::create(event, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        if (screenshotHandler->do_image_capture) screenshotHandler->screenshot_image(window);
        if (screenshotHandler->do_depth_capture) screenshotHandler->screenshot_depth(window);

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
