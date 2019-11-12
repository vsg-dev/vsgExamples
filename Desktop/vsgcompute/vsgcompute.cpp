#include <vsg/all.h>

#include <iostream>
#include <chrono>

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);
    auto width = arguments.value(1024u, "--width");
    auto height = arguments.value(1024u, "--height");
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto workgroupSize = arguments.value<uint32_t>(32, "-w");
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
    vsg::ref_ptr<vsg::ShaderStage> computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", vsg::findFile("shaders/comp.spv", searchPaths));
    if (!computeStage)
    {
        std::cout<<"Error : No shader loaded."<<std::endl;
        return 1;
    }

    auto dimensions = vsg::uintArray::create({width, height, workgroupSize});
    computeStage->setSpecializationMapEntries(vsg::ShaderStage::SpecializationMapEntries{{0, 0, 4}, {1, 4, 4}, {2, 8, 4}});
    computeStage->setSpecializationData(dimensions);

    vsg::Names validatedNames = vsg::validateInstancelayerNames(requestedLayers);

    vsg::ref_ptr<vsg::Instance> instance = vsg::Instance::create(instanceExtensions, validatedNames);
    vsg::ref_ptr<vsg::PhysicalDevice> physicalDevice = vsg::PhysicalDevice::create(instance, VK_QUEUE_COMPUTE_BIT);
    vsg::ref_ptr<vsg::Device> device = vsg::Device::create(physicalDevice, validatedNames, deviceExtensions);
    if (!device)
    {
        std::cout<<"Unable to create required Vulkan Device."<<std::endl;
        return 1;
    }

    // get the queue for the compute commands
    auto computeQueue = device->getQueue(physicalDevice->getComputeFamily());

    // allocate output storage buffer
    VkDeviceSize bufferSize = sizeof(vsg::vec4) * width * height;
    vsg::ref_ptr<vsg::Buffer> buffer =  vsg::Buffer::create(device, bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    vsg::ref_ptr<vsg::DeviceMemory>  bufferMemory = vsg::DeviceMemory::create(device, buffer,  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    buffer->bind(bufferMemory, 0);

    // set up DescriptorSetLayout, DecriptorSet and BindDescriptorSets
    vsg::DescriptorSetLayoutBindings descriptorBindings { {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr} };
    vsg::DescriptorSetLayouts descriptorSetLayouts { vsg::DescriptorSetLayout::create(descriptorBindings) };
    vsg::Descriptors descriptors { vsg::DescriptorBuffer::create(vsg::BufferDataList{vsg::BufferData(buffer, 0, bufferSize)}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) };

    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(descriptorSetLayouts, descriptors);
    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, vsg::PushConstantRanges{});
    vsg::ref_ptr<vsg::BindDescriptorSets> bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, vsg::DescriptorSets{descriptorSet});

    // set up the compute pipeline
    vsg::ref_ptr<vsg::ComputePipeline> pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
    vsg::ref_ptr<vsg::BindComputePipeline> bindPipeline = vsg::BindComputePipeline::create(pipeline);

    // assign to a CommandGraph that binds the Pipeline and DescritorSets and calls Dispatch
    vsg::ref_ptr<vsg::StateGroup> commandGraph = vsg::StateGroup::create();
    commandGraph->add(bindPipeline);
    commandGraph->add(bindDescriptorSets);
    commandGraph->addChild(vsg::Dispatch::create(uint32_t(ceil(float(width)/float(workgroupSize))), uint32_t(ceil(float(height)/float(workgroupSize))), 1));

    // compile the Vulkan objects
    vsg::CompileTraversal compileTraversal(device);
    compileTraversal.context.commandPool = vsg::CommandPool::create(device, physicalDevice->getComputeFamily());
    compileTraversal.context.descriptorPool = vsg::DescriptorPool::create(device, 1, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}});

    commandGraph->accept(compileTraversal);

    // setup fence
    vsg::ref_ptr<vsg::Fence> fence = vsg::Fence::create(device);

    auto startTime =std::chrono::steady_clock::now();

    // submit commands
    vsg::submitCommandsToQueue(device, compileTraversal.context.commandPool, fence, 100000000000, computeQueue, [&](vsg::CommandBuffer& commandBuffer)
    {
        vsg::DispatchTraversal dispatchTraversals(&commandBuffer);
        commandGraph->accept(dispatchTraversals);
    });

    auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::steady_clock::now()-startTime).count();
    std::cout<<"Time to run commands "<<time<<"ms"<<std::endl;

    if (!outputFilename.empty())
    {
        // Map the buffer memory and assign as a vec4Array2D that will automatically unmap itself on destruction.
        auto image = vsg::MappedData<vsg::vec4Array2D>::create(bufferMemory, 0, 0, width, height); // deviceMemory, offset, flags and dimensions
        image->setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);

        if (outputAsFloat)
        {
            vsg::write(image, outputFilename);
        }
        else
        {
            // create a unsigned byte version of the image and then copy the texels across converting colours from float to unsigned byte.
            auto dest = vsg::ubvec4Array2D::create(width, height);
            dest->setFormat(VK_FORMAT_R8G8B8A8_UNORM);
            using component_type = uint8_t;
            auto c_itr = dest->begin();
            for(auto& colour : *image)
            {
                (c_itr++)->set(static_cast<component_type>(colour.r*255.0), static_cast<component_type>(colour.g*255.0), static_cast<component_type>(colour.b*255.0), static_cast<component_type>(colour.a*255.0));
            }

            vsg::write(dest, outputFilename);
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
