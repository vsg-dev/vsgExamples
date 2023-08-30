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

    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    if (arguments.read("--msaa")) samples = VK_SAMPLE_COUNT_8_BIT;
    bool enableGeometryShader = arguments.read("--gs");

    uint32_t vulkanVersion = VK_API_VERSION_1_0;
    if (samples != VK_SAMPLE_COUNT_1_BIT) vulkanVersion = VK_API_VERSION_1_2;

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
    auto context = vsg::Context::create(device);
    context->commandPool = vsg::CommandPool::create(device, queueFamily);
    context->graphicsQueue = device->getQueue(queueFamily);

    // test 1
    {
        vsg::info("---- Test 1 : Sharing Sampler between ImageInfo/InmageView/Image instances ----");

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

    return 0;
}
