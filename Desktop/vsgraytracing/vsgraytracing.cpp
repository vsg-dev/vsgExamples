#include <vsg/all.h>
#include <iostream>

#include "../vsgexp/ExperimentalViewer.h"

vsg::ImageData createImageView(vsg::Context& context, const VkImageCreateInfo& imageCreateInfo, VkImageAspectFlags aspectFlags, VkImageLayout targetImageLayout)
{
    vsg::Device* device = context.device;

    vsg::ref_ptr<vsg::Image> image;

    image = vsg::Image::create(device, imageCreateInfo);

    // get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*device, *image, &memRequirements);

    // allocate memory with out export memory info extension
    auto[deviceMemory, offset] = context.deviceMemoryBufferPools->reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (!deviceMemory)
    {
        std::cout << "Warning: Failed allocate memory to reserve slot" << std::endl;
        return vsg::ImageData();
    }

    image->bind(deviceMemory, offset);

    vsg::ref_ptr<vsg::ImageView> imageview = vsg::ImageView::create(device, image, VK_IMAGE_VIEW_TYPE_2D, imageCreateInfo.format, aspectFlags);

    return vsg::ImageData(nullptr, imageview, targetImageLayout);
}

struct RayTracingUniform
{
    vsg::mat4 viewInverse;
    vsg::mat4 projInverse;
};

class RayTracingUniformValue : public vsg::Inherit<vsg::Value<RayTracingUniform>, RayTracingUniformValue>
{
public:
    RayTracingUniformValue() {}
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(1280, 720), {"--window", "-w"});
    auto useNewViewer = arguments.read("--new");
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // create the viewer and assign window(s) to it
    auto viewer = vsg::ExperimentalViewer::create();

    auto windowTraits = vsg::Window::Traits::create();
    windowTraits->windowTitle = "vsgraytracing";
    windowTraits->debugLayer = true;
    windowTraits->apiDumpLayer = false;
    windowTraits->width = width;
    windowTraits->height = height;

    windowTraits->instanceExtensionNames =
    {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    windowTraits->deviceExtensionNames =
    {
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_NV_RAY_TRACING_EXTENSION_NAME
    };

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);


    // query raytracing properties of device
    VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties;
    rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
    rayTracingProperties.pNext = nullptr;
    VkPhysicalDeviceProperties2 deviceProps2;
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &rayTracingProperties;
    vkGetPhysicalDeviceProperties2(*window->device()->getPhysicalDevice(), &deviceProps2);

    // for convenience create a compile context for creating our storage image
    vsg::CompileTraversal compile(window->device());
    compile.context.commandPool = vsg::CommandPool::create(window->device(), window->device()->getPhysicalDevice()->getGraphicsFamily());
    compile.context.renderPass = window->renderPass();
    compile.context.graphicsQueue = window->device()->getQueue(window->device()->getPhysicalDevice()->getGraphicsFamily());


    // load shaders

    const uint32_t shaderIndexRaygen = 0;
    const uint32_t shaderIndexMiss = 1;
    const uint32_t shaderIndexClosestHit = 2;

    vsg::ref_ptr<vsg::ShaderStage> raygenShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_NV, "main", vsg::findFile("shaders/simple_raygen.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> missShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_NV, "main", vsg::findFile("shaders/simple_miss.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> closesthitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, "main", vsg::findFile("shaders/simple_closesthit.spv", searchPaths));

    if (!raygenShader || !missShader || !closesthitShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return 1;
    }

    auto shaderStages = vsg::ShaderStages{ raygenShader, missShader, closesthitShader };


    // set up shader groups
    auto raygenShaderGroup = vsg::RayTracingShaderGroup::create();
    raygenShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    raygenShaderGroup->generalShader = 0;

    auto missShaderGroup = vsg::RayTracingShaderGroup::create();
    missShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    missShaderGroup->generalShader = 1;

    auto closestHitShaderGroup = vsg::RayTracingShaderGroup::create();
    closestHitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
    closestHitShaderGroup->closestHitShader = 2;

    auto shaderGroups = vsg::RayTracingShaderGroups{ raygenShaderGroup, missShaderGroup, closestHitShaderGroup };


    // acceleration structures
    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
    {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 0.0f,  1.0f, 0.0f}
    });

    auto indices = vsg::uintArray::create(
    {
        0, 1, 2
    });

    // create acceleration geometry
    auto accelGeometry = vsg::AccelerationGeometry::create();
    accelGeometry->_verts = vertices;
    accelGeometry->_indices = indices;

    // create bottom level acceleration structure using accel geom
    auto blas = vsg::BottomLevelAccelerationStructure::create(window->device());
    blas->_geometries.push_back(accelGeometry);

    // create top level acceleration structure
    auto tlas = vsg::TopLevelAccelerationStructure::create(window->device());

    // add geometry instance to top level acceleration structure that uses the bottom level structure
    auto geominstance = vsg::GeometryInstance::create();
    geominstance->_accelerationStructure = blas;
    geominstance->_transform = vsg::mat4();

    tlas->_geometryInstances.push_back(geominstance);


    // create storage image to render into
    VkImageCreateInfo storageImageCreateInfo;
    storageImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    storageImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    storageImageCreateInfo.format = VK_FORMAT_B8G8R8A8_UNORM;//VK_FORMAT_R8G8B8A8_UNORM;
    storageImageCreateInfo.extent.width = width;
    storageImageCreateInfo.extent.height = height;
    storageImageCreateInfo.extent.depth = 1;
    storageImageCreateInfo.mipLevels = 1;
    storageImageCreateInfo.arrayLayers = 1;
    storageImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    storageImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    storageImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    storageImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    storageImageCreateInfo.flags = 0;
    storageImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    storageImageCreateInfo.queueFamilyIndexCount = 0;
    storageImageCreateInfo.pNext = nullptr;

    vsg::ImageData storageImageData = createImageView(compile.context, storageImageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);

    // create camera matrices and uniform for shader
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 0.0, -2.5), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));

    auto raytracingUniformValues = new RayTracingUniformValue();
    perspective->get_inverse(raytracingUniformValues->value().projInverse);
    lookAt->get_inverse(raytracingUniformValues->value().viewInverse);

    vsg::ref_ptr<RayTracingUniformValue> raytracingUniform(raytracingUniformValues);

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV, nullptr}, // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV, nullptr},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV, nullptr}
    };

    vsg::DescriptorSetLayouts descriptorSetLayouts{vsg::DescriptorSetLayout::create(descriptorBindings)};

    // create DescriptorSets and binding to bind our TopLevelAcceleration structure, storage image and camra matrix uniforms
    auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{tlas}, 0, 0);

    auto storageImageDescriptor = vsg::DescriptorImageView::create(storageImageData, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    auto raytracingUniformDescriptor = vsg::DescriptorBuffer::create(raytracingUniform, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    raytracingUniformDescriptor->copyDataListToBuffers();

    auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, vsg::PushConstantRanges{});
    auto raytracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStages, shaderGroups);
    auto bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(raytracingPipeline);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayouts, vsg::Descriptors{ accelDescriptor, storageImageDescriptor, raytracingUniformDescriptor });
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, raytracingPipeline->getPipelineLayout(), 0, vsg::DescriptorSets{descriptorSet});

    // state group to bind the pipeline and descriptorset
    auto scenegraph = vsg::Commands::create();
    scenegraph->addChild(bindRayTracingPipeline);
    scenegraph->addChild(bindDescriptorSets);

    // setup tracing of rays
    auto traceRays = vsg::TraceRays::create();
    traceRays->raygen = raygenShaderGroup;
    traceRays->missShader = missShaderGroup;
    traceRays->hitShader = closestHitShaderGroup;
    traceRays->width = width;
    traceRays->height = height;
    traceRays->depth = 1;

    scenegraph->addChild(traceRays);

    // camera related details
    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    if (useNewViewer)
    {
        // set up commandGraph to rendering viewport
        auto commandGraph = vsg::CommandGraph::create(window->device(), window->physicalDevice()->getGraphicsFamily());
        for(size_t i = 0; i < window->numFrames(); ++i)
        {
            commandGraph->commandBuffers.emplace_back(window->commandBuffer(i));
        }

        auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(storageImageData._imageView, window);

        commandGraph->addChild(scenegraph);
        commandGraph->addChild(copyImageViewToWindow);


        auto renderFinishedSemaphore = vsg::Semaphore::create(window->device());

        // set up RecordAndSubmitTask with CommandBuffer and signals
        auto recordAndSubmitTask = vsg::RecordAndSubmitTask::create();
        recordAndSubmitTask->commandGraphs.emplace_back(commandGraph);
        recordAndSubmitTask->signalSemaphores.emplace_back(renderFinishedSemaphore);
        recordAndSubmitTask->windows = viewer->windows();
        recordAndSubmitTask->queue = window->device()->getQueue(window->physicalDevice()->getGraphicsFamily());
        viewer->recordAndSubmitTasks.emplace_back(recordAndSubmitTask);

        auto presentation = vsg::Presentation::create();
        presentation->waitSemaphores.emplace_back(renderFinishedSemaphore);
        presentation->windows = viewer->windows();
        presentation->queue = window->device()->getQueue(window->physicalDevice()->getPresentFamily());
        viewer->presentation = presentation;

        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame())
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }
    }
    else
    {
        // add a GraphicsStage to the Window to do dispatch of the command graph to the commnad buffer(s)
        //window->addStage(vsg::RayTracingStage::create(scenegraph, storageImageData._imageView, VkExtent2D{ width, height }, camera));

        // compile the Vulkan objects
        viewer->compile();

        // main frame loop
        while (viewer->advanceToNextFrame())
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
