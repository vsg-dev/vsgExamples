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

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    bool debugLayer = true;
    auto apiDumpLayer = arguments.read({"--api", "-a"});

    uint32_t vulkanVersion = VK_API_VERSION_1_0;

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

    auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures);
    auto context = vsg::Context::create(device);
    context->commandPool = vsg::CommandPool::create(device, queueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    context->graphicsQueue = device->getQueue(queueFamily);

    // test 1
    {
        vsg::info("---- Test 1 : Sharing Sampler between ImageInfo/ImageView/Image instances ----");

        auto sampler = vsg::Sampler::create();
        sampler->maxLod = VK_LOD_CLAMP_NONE;

        // uncompressed data
        auto textureData1 = vsg::ubvec4Array2D::create(32, 32, vsg::Data::Properties(VK_FORMAT_R8G8B8A8_UNORM));
        // compressed data
        auto textureData2 = vsg::block64Array2D::create(32, 32, vsg::Data::Properties(VK_FORMAT_BC4_UNORM_BLOCK));
        textureData2->properties.blockWidth = 8;
        textureData2->properties.blockHeight = 8;

        auto imageInfo1 = vsg::ImageInfo::create(sampler, textureData1);
        auto imageInfo2 = vsg::ImageInfo::create(sampler, textureData2);

        if (imageInfo1->sampler->maxLod != VK_LOD_CLAMP_NONE)
            vsg::warn("test 1 failed: shared sampler has been changed");
        else
            vsg::info("test 1 passed");

        // test compile(..) and copy(..) implementations
        auto descriptorImage1 = vsg::DescriptorImage::create(imageInfo1);
        auto descriptorImage2 = vsg::DescriptorImage::create(imageInfo2);

        vsg::CollectResourceRequirements collectResourceRequirements;
        descriptorImage1->accept(collectResourceRequirements);
        descriptorImage2->accept(collectResourceRequirements);

        descriptorImage1->compile(*context);
        descriptorImage2->compile(*context);
        context->record();
        context->waitForCompletion();

        vsg::info("End of test 1\n");
    }

    // test2
    {
        vsg::info("---- Test 2 : Automatic disabling of mipmapping when using compressed image formats. ----");

        auto textureData = vsg::block64Array2D::create(32, 32, vsg::Data::Properties(VK_FORMAT_BC4_UNORM_BLOCK));

        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(*(device->getPhysicalDevice()), textureData->properties.format, &props);
        const bool isBlitPossible = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) > 0 && (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) > 0;
        if (isBlitPossible)
        {
            vsg::warn("unable to proceed with test2: no format without blit support found.");
            return 0;
        }

        // simulates an uncompressed format without blit support (hard to come by, but Vulkan allows it)
        textureData->properties.blockWidth = 1;
        textureData->properties.blockHeight = 1;
        auto sampler = vsg::Sampler::create();
        sampler->maxLod = VK_LOD_CLAMP_NONE;

        auto imageInfo = vsg::ImageInfo::create(sampler, textureData);
        auto descriptorImage = vsg::DescriptorImage::create(imageInfo);

        vsg::CollectResourceRequirements collectResourceRequirements;
        descriptorImage->accept(collectResourceRequirements);

        if (imageInfo->imageView->image->mipLevels > 1)
        {
            vsg::warn("test2 failed: image will be created with ", imageInfo->imageView->image->mipLevels, " mipLevels, but we have no way to fill them in.");
        }
        else
            vsg::info("test2 passed");

        vsg::info("End of test 2\n");
    }

    // test3
    {
        vsg::info("---- Test 3 : DescriptorPool reservation in vsg::Context ----");

        vsg::ResourceRequirements requirements;
        requirements.descriptorTypeMap[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 1;

        vsg::info("Test 3, part 1");
        requirements.externalNumDescriptorSets = 1;
        context->reserve(requirements);

        vsg::info("Test 3, part 2");
        requirements.externalNumDescriptorSets = 2;
        requirements.descriptorTypeMap[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 2;
        context->reserve(requirements);

        // vkCreateDescriptorPool(): pCreateInfo->pPoolSizes[0].descriptorCount is not greater than 0. The Vulkan spec states: descriptorCount must be greater than 0 (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkDescriptorPoolSize-descriptorCount-00302)
        vsg::info("End of test 3\n");
    }

    // test4
    {
        vsg::info("---- Test 4 : Array data mipmaps ----");
        uint8_t mipLevels = 3;
        uint32_t width = 32;
        uint32_t height = 32;
        uint32_t layers = 32;
        //size_t valueCount = vsg::Data::computeValueCountIncludingMipmaps(width, height, 1, mipLevels, layers);
        size_t valueCount = vsg::Data::computeValueCountIncludingMipmaps(width, height, 1, mipLevels) * layers; // use deprecated function for compatibility with VSG 1.0.9 and below

        std::unique_ptr<vsg::ubvec4> raw (new vsg::ubvec4[valueCount]);
        auto value = vsg::ubvec4(0xff, 0xff, 0xff, 0xff);
        for (size_t v = 0; v < valueCount; ++v)
            raw.get()[v] = value;

        vsg::Data::Properties properties(VK_FORMAT_R8G8B8A8_UNORM);
        properties.maxNumMipmaps = mipLevels;
        properties.imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        auto array = vsg::ubvec4Array3D::create(width, height, layers, reinterpret_cast<vsg::ubvec4*>(raw.release()), properties);

        auto sampler = vsg::Sampler::create();
        sampler->maxLod = VK_LOD_CLAMP_NONE;
        auto imageInfo = vsg::ImageInfo::create(sampler, array);
        imageInfo->imageView->image->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        // test compile(..) and copy(..) implementations
        auto descriptorImage = vsg::DescriptorImage::create(imageInfo);
        descriptorImage->compile(*context);
        context->record();
        context->waitForCompletion();

        // take a screenshot of the last mip level and array layer to verify that the data has been uploaded completely
        auto sourceImage = imageInfo->imageView->image;
        auto targetImageFormat = properties.format;
        uint32_t baseArrayLayer = layers-1;
        uint32_t mipLevel = mipLevels-1;
        width = width / (1 << mipLevel);
        height = height / (1 << mipLevel);

        // start of vsgscreenshot code
        //
        // 2) create image to write to
        //
        auto destinationImage = vsg::Image::create();
        destinationImage->imageType = VK_IMAGE_TYPE_2D;
        destinationImage->format = properties.format;
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

        // 3.b) transition sourceImage to transfer source initialLayout
        auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_MEMORY_READ_BIT,                                     // srcAccessMask
            VK_ACCESS_TRANSFER_READ_BIT,                                   // dstAccessMask
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                               // oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // newLayout
            VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
            sourceImage,                                                   // image
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1, baseArrayLayer, 1} // subresourceRange
        );

        auto cmd_transitionForTransferBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                       // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                       // dstStageMask
            0,                                                    // dependencyFlags
            transitionDestinationImageToDestinationLayoutBarrier, // barrier
            transitionSourceImageToTransferSourceLayoutBarrier    // barrier
        );

        commands->addChild(cmd_transitionForTransferBarrier);

        //if (supportsBlit)
        {
            // 3.c.1) if blit using vkCmdBlitImage
            VkImageBlit region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.mipLevel = mipLevel;
            region.srcSubresource.baseArrayLayer = baseArrayLayer;
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

        auto cmd_transitionFromTransferBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // srcStageMask
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
            0,                                             // dependencyFlags
            transitionDestinationImageToMemoryReadBarrier // barrier
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
        vsg::ref_ptr<vsg::ubvec4Array2D> imageData;
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
        // end of vsgscreenshot code

        for (uint32_t x=0; x<width; ++x)
        {
            for (uint32_t y=0; y<height; ++y)
            {
                if (imageData->at(x, y) != value)
                {
                    vsg::warn("array_data_mipmaps test failed: downloaded data at (", x, ",", y, ") does not match input data.");
                    return 0;
                }
            }
        }
        vsg::info("array_data_mipmaps test passed");
        vsg::info("End of test 4\n");
    }
    return 0;
}
