#pragma once

// Copies the last presented swapchain image into a binary PPM (after deviceWaitIdle).
// Reduced from vsgExamples/examples/app/vsgscreenshot.

#include <vsg/all.h>

#include <cstring>
#include <fstream>
#include <string>

inline bool captureWindow(vsg::ref_ptr<vsg::Window> window, const std::string& filename)
{
    const uint32_t width = window->extent2D().width;
    const uint32_t height = window->extent2D().height;
    auto device = window->getDevice();
    auto physicalDevice = window->getPhysicalDevice();
    auto sourceImage = window->imageView(window->imageIndex(0))->image;
    const VkFormat sourceFormat = window->getSwapchain()->getImageFormat();

    auto destination = vsg::Image::create();
    destination->imageType = VK_IMAGE_TYPE_2D;
    destination->format = sourceFormat;
    destination->extent = VkExtent3D{width, height, 1};
    destination->arrayLayers = 1;
    destination->mipLevels = 1;
    destination->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    destination->samples = VK_SAMPLE_COUNT_1_BIT;
    destination->tiling = VK_IMAGE_TILING_LINEAR;
    destination->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    destination->compile(device);
    auto memory = vsg::DeviceMemory::create(device, destination->getMemoryRequirements(device->deviceID),
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    destination->bind(memory, 0);

    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto commands = vsg::Commands::create();
    commands->addChild(vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        vsg::ImageMemoryBarrier::create(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED,
                                        VK_QUEUE_FAMILY_IGNORED, destination, range),
        vsg::ImageMemoryBarrier::create(VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, sourceImage, range)));
    VkImageCopy region{};
    region.srcSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = region.srcSubresource;
    region.extent = VkExtent3D{width, height, 1};
    auto copy = vsg::CopyImage::create();
    copy->srcImage = sourceImage;
    copy->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copy->dstImage = destination;
    copy->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy->regions.push_back(region);
    commands->addChild(copy);
    commands->addChild(vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        vsg::ImageMemoryBarrier::create(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, destination, range),
        vsg::ImageMemoryBarrier::create(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, sourceImage, range)));

    auto fence = vsg::Fence::create(device);
    const auto queueFamily = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    vsg::submitCommandsToQueue(vsg::CommandPool::create(device, queueFamily), fence, 100000000000,
                               device->getQueue(queueFamily),
                               [&](vsg::CommandBuffer& commandBuffer) { commands->record(commandBuffer); });

    VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(*device, destination->vk(device->deviceID), &subResource, &layout);
    auto mapped = vsg::MappedData<vsg::ubyteArray>::create(memory, layout.offset, 0, vsg::Data::Properties{},
                                                           layout.rowPitch * height);

    const bool bgra = sourceFormat == VK_FORMAT_B8G8R8A8_SRGB || sourceFormat == VK_FORMAT_B8G8R8A8_UNORM;
    std::ofstream out(filename, std::ios::binary);
    out << "P6\n" << width << " " << height << "\n255\n";
    for (uint32_t row = 0; row < height; ++row)
    {
        const uint8_t* pixel = static_cast<const uint8_t*>(mapped->dataPointer(row * layout.rowPitch));
        for (uint32_t x = 0; x < width; ++x, pixel += 4)
        {
            const char rgb[3] = {static_cast<char>(pixel[bgra ? 2 : 0]), static_cast<char>(pixel[1]),
                                 static_cast<char>(pixel[bgra ? 0 : 2])};
            out.write(rgb, 3);
        }
    }
    return static_cast<bool>(out);
}
