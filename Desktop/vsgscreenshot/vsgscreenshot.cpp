#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

#include "../vsgviewer/AnimationPath.h"


class ScreenshotHandler : public vsg::Inherit<vsg::Visitor, ScreenshotHandler>
{
public:

    uint32_t sourceSwapChainIndex = 0;

    ScreenshotHandler()
    {
    }

    void apply(vsg::KeyPressEvent& keyPress) override
    {
        if (keyPress.keyBase=='s')
        {
            screenshot(keyPress.window);
        }
    }

    void apply(vsg::ButtonPressEvent& buttonPressEvent) override
    {
        if (buttonPressEvent.button==1)
        {
            vsg::ref_ptr<vsg::Window> window = buttonPressEvent.window;
            std::cout<<"Need to read pixel at " <<buttonPressEvent.x<<", "<<buttonPressEvent.y<<", "<<window<<std::endl;
        }
    }

    void printInfo(vsg::ref_ptr<vsg::Window> window)
    {
        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain();
        std::cout<<"\nNeed to take screenshot " <<window<<std::endl;
        std::cout<<"    device = " <<device<<std::endl;
        std::cout<<"    physicalDevice = " <<physicalDevice<<std::endl;
        std::cout<<"    swapchain = " <<swapchain<<std::endl;
        std::cout<<"        swapchain->getImageFormat() = " <<swapchain->getImageFormat()<<std::endl;
        std::cout<<"        swapchain->getExtent() = " <<swapchain->getExtent().width<<", "<<swapchain->getExtent().height<<std::endl;

        for(auto& imageView : swapchain->getImageViews())
        {
            std::cout<<"        imageview = " <<imageView<<std::endl;
        }

        std::cout<<"    numFrames() = "<<window->numFrames()<<std::endl;
        for(size_t i = 0; i<window->numFrames(); ++i)
        {
            std::cout<<"        imageview["<<i<<"] = " <<window->imageView(i)<<std::endl;
            std::cout<<"        framebuffer["<<i<<"] = " <<window->framebuffer(i)<<std::endl;
        }

        std::cout<<"    nextImageIndex() = "<<window->nextImageIndex()<<std::endl;

        std::cout<<"    surfaceFormat() = "<<window->surfaceFormat().format<<", "<<window->surfaceFormat().colorSpace<<std::endl;
        std::cout<<"    depthFormat() = "<<window->depthFormat()<<std::endl;
    }

    void screenshot(vsg::ref_ptr<vsg::Window> window)
    {
        printInfo(window);

        auto width = window->extent2D().width;
        auto height = window->extent2D().height;

        auto device = window->getDevice();
        auto physicalDevice = window->getPhysicalDevice();
        auto swapchain = window->getSwapchain();

        std::cout<<"sourceSwapChainIndex = "<<sourceSwapChainIndex<<std::endl;

        vsg::ref_ptr<vsg::Image> sourceImage(window->imageView(sourceSwapChainIndex)->getImage());

        VkFormat sourceImageFormat = swapchain->getImageFormat();
        VkFormat targetImageFormat = sourceImageFormat;

    // 1)
        VkFormatProperties srcFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), swapchain->getImageFormat(), &srcFormatProperties);

        VkFormatProperties destFormatProperties;
        vkGetPhysicalDeviceFormatProperties(*(physicalDevice), targetImageFormat, &destFormatProperties);

        bool supportsBlit = ((srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0) &&
                            ((destFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0);


        if (supportsBlit)
        {
            // we can automatically convert the image format when blit, so take advantage of it to ensure RGBA
            targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        }
        //supportsBlit = true;
        std::cout<<"supportsBlit = "<<supportsBlit<<std::endl;

    // 2)  crete imageview

        VkImageCreateInfo imageCreateInfo = {};
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

        auto deviceMemory = vsg::DeviceMemory::create(device, destinationImage->getMemoryRequirements(),  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        destinationImage->bind(deviceMemory, 0);

        std::cout<<"destinationImage = "<<destinationImage<<std::endl;

    // 3) create command buffer and submit to graphcis queue

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

        auto cmd_transitionDestinationImageToDestinationLayoutBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
            0, // dependencyFlags
            transitionDestinationImageToDestinationLayoutBarrier // barrier
        );

        commands->addChild(cmd_transitionDestinationImageToDestinationLayoutBarrier);


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

        auto cmd_transitionSourceImageToTransferSourceLayoutBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
            0, // dependencyFlags
            transitionDestinationImageToDestinationLayoutBarrier // barrier
        );

        commands->addChild(cmd_transitionSourceImageToTransferSourceLayoutBarrier);

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

        auto cmd_transitionDestinationImageToMemoryReadBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
            0, // dependencyFlags
            transitionDestinationImageToMemoryReadBarrier // barrier
        );

        commands->addChild(cmd_transitionDestinationImageToMemoryReadBarrier);

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

        auto cmd_transitionSourceImageBackToPresentBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
            0, // dependencyFlags
            transitionSourceImageBackToPresentBarrier // barrier
        );

        commands->addChild(cmd_transitionSourceImageToTransferSourceLayoutBarrier);

        auto fence = vsg::Fence::create(device);
        auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
        auto queue = device->getQueue(queueFamilyIndex);

        vsg::submitCommandsToQueue(device, commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer)
        {
            commands->dispatch(commandBuffer);
        });


    // 4) map image and copy

        VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(*device, *destinationImage, &subResource, &subResourceLayout);

        // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
        auto imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, subResourceLayout.offset, 0, width, height); // deviceMemory, offset, flags and dimensions

        imageData->setFormat(targetImageFormat);

        vsg::Path outputFilename("screenshot.vsgt");
        vsg::write(imageData, outputFilename);
    }
};


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgscreenshot";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    auto numFrames = arguments.value(-1, "-f");
    auto useDatabasePager = arguments.read("--pager");
    auto maxPageLOD = arguments.value(-1, "--max-plod");
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    arguments.read("--samples", windowTraits->samples);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

#ifdef USE_VSGXCHANGE
    // add use of vsgXchange's support for reading and writing 3rd party file formats
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif

    vsg::Path filename;
    if (argc>1) filename = arguments[1];

    auto vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
    if (!vsg_scene)
    {
        std::cout<<"Please specify a 3d model file on the command line."<<std::endl;
        return 1;
    }


    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
    if (vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel")); ellipsoidModel)
    {
        perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, 0.0);
    }
    else
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 4.5);
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

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

    // Adde ScreenshotHandler to respond to keyboard and mosue events.
    auto screenshotHandler = ScreenshotHandler::create();
    viewer->addEventHandler(screenshotHandler);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph}, databasePager);

    viewer->compile();


    screenshotHandler->sourceSwapChainIndex = 0;

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames<0 || (numFrames--)>0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        screenshotHandler->sourceSwapChainIndex = window->nextImageIndex();

        viewer->present();

    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
