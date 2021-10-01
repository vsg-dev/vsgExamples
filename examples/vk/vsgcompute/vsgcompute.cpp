#include <vsg/all.h>

#include <chrono>
#include <iostream>

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    auto width = arguments.value(1024, "--width");
    auto height = arguments.value(1024, "--height");
    auto debugLayer = arguments.read({"--debug", "-d"});
    auto apiDumpLayer = arguments.read({"--api", "-a"});
    auto workgroupSize = arguments.value(32, "-w");
    auto outputFilename = arguments.value<std::string>("", "-o");
    auto outputAsFloat = arguments.read("-f");
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    vsg::Names instanceExtensions;
    vsg::Names requestedLayers;
    vsg::Names deviceExtensions;
    if (debugLayer)
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        requestedLayers.push_back("VK_LAYER_LUNARG_standard_validation");
        if (apiDumpLayer) requestedLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }

    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", vsg::findFile("shaders/comp.spv", searchPaths));
    if (!computeStage)
    {
        std::cout << "Error : No shader loaded." << std::endl;
        return 1;
    }

    computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(width)},
        {1, vsg::intValue::create(height)},
        {2, vsg::intValue::create(workgroupSize)}};

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    // get the physical device that supports the required compute queue
    auto instance = vsg::Instance::create(instanceExtensions, validatedNames);
    auto [physicalDevice, computeQueueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_COMPUTE_BIT);
    if (!physicalDevice || computeQueueFamily < 0)
    {
        std::cout << "No vkPhysicalDevice available that supports compute." << std::endl;
        return 1;
    }

    // create the logical device with specified queue, layers and extensions
    vsg::QueueSettings queueSettings{vsg::QueueSetting{computeQueueFamily, {1.0}}};
    auto device = vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions);

    // get the queue for the compute commands
    auto computeQueue = device->getQueue(computeQueueFamily);

    // allocate output storage buffer
    VkDeviceSize bufferSize = sizeof(vsg::vec4) * width * height;

    auto buffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto bufferMemory = buffer->getDeviceMemory(device->deviceID);

    // set up DescriptorSetLayout, DecriptorSet and BindDescriptorSets
    vsg::DescriptorSetLayoutBindings descriptorBindings{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::Descriptors descriptors{vsg::DescriptorBuffer::create(vsg::BufferInfoList{vsg::BufferInfo::create(buffer, 0, bufferSize)}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)};
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

    // set up the compute pipeline
    auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
    auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

    // assign to a CommandGraph that binds the Pipeline and DescritorSets and calls Dispatch
    auto commandGraph = vsg::Commands::create();
    commandGraph->addChild(bindPipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(vsg::Dispatch::create(uint32_t(ceil(float(width) / float(workgroupSize))), uint32_t(ceil(float(height) / float(workgroupSize))), 1));

    // compile the Vulkan objects
    vsg::CompileTraversal compileTraversal(device);
    compileTraversal.context.commandPool = vsg::CommandPool::create(device, computeQueueFamily);
    compileTraversal.context.descriptorPool = vsg::DescriptorPool::create(device, 1, vsg::DescriptorPoolSizes{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}});

    commandGraph->accept(compileTraversal);

    // setup fence
    auto fence = vsg::Fence::create(device);

    auto startTime = std::chrono::steady_clock::now();

    // submit commands
    vsg::submitCommandsToQueue(device, compileTraversal.context.commandPool, fence, 100000000000, computeQueue, [&](vsg::CommandBuffer& commandBuffer) {
        commandGraph->record(commandBuffer);
    });

    auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - startTime).count();
    std::cout << "Time to run commands " << time << "ms" << std::endl;

    if (!outputFilename.empty())
    {
        // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
        auto image = vsg::MappedData<vsg::vec4Array2D>::create(bufferMemory, 0, 0, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT}, width, height); // deviceMemory, offset, flags an d dimensions

        if (outputAsFloat)
        {
            vsg::write(image, outputFilename);
        }
        else
        {
            // create a unsigned byte version of the image and then copy the texels across converting colours from float to unsigned byte.
            auto dest = vsg::ubvec4Array2D::create(width, height, vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});

            using component_type = uint8_t;
            auto c_itr = dest->begin();
            for (auto& colour : *image)
            {
                (c_itr++)->set(static_cast<component_type>(colour.r * 255.0), static_cast<component_type>(colour.g * 255.0), static_cast<component_type>(colour.b * 255.0), static_cast<component_type>(colour.a * 255.0));
            }

            vsg::write(dest, outputFilename);
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
