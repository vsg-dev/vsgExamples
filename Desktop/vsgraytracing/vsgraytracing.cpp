#include <vsg/all.h>
#include <iostream>

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
    auto filename = arguments.value(std::string(), "-i");
    if (arguments.read("-m")) filename = "models/raytracing_scene.vsgt";
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);


    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "vsgraytracing";
    windowTraits->debugLayer = true;
    windowTraits->apiDumpLayer = false;
    windowTraits->width = width;
    windowTraits->height = height;
    windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    windowTraits->imageAvailableSemaphoreWaitFlag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // enable the transfer bit as we want to copy the raytraced image to swapchain

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


    // for convenience create a compile context for creating our storage image
    auto queueFamily = window->physicalDevice()->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    vsg::CompileTraversal compile(window->device());
    compile.context.renderPass = window->renderPass();
    compile.context.commandPool = vsg::CommandPool::create(window->device(), queueFamily);
    compile.context.graphicsQueue = window->device()->getQueue(queueFamily);


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

    // create camera matrices and uniform for shader
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0);
    vsg::ref_ptr<vsg::LookAt> lookAt;

    vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;
    if (filename.empty())
    {
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
        accelGeometry->verts = vertices;
        accelGeometry->indices = indices;

        // create bottom level acceleration structure using accel geom
        auto blas = vsg::BottomLevelAccelerationStructure::create(window->device());
        blas->geometries.push_back(accelGeometry);

        // create top level acceleration structure
        tlas = vsg::TopLevelAccelerationStructure::create(window->device());

        // add geometry instance to top level acceleration structure that uses the bottom level structure
        auto geominstance = vsg::GeometryInstance::create();
        geominstance->accelerationStructure = blas;
        geominstance->transform = vsg::mat4();

        tlas->geometryInstances.push_back(geominstance);

        lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 0.0, -2.5), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
    }
    else
    {
        vsg::Path path = vsg::fileExists(filename) ? filename : vsg::findFile(filename, searchPaths);
        if (path.empty())
        {
            std::cout<<"Could not find file "<<filename<<std::endl;
            return 1;
        }

        auto loaded_scene = vsg::read_cast<vsg::Node>(path);
        if (!loaded_scene) loaded_scene = vsg::read_cast<vsg::Node>(filename);

        if (!loaded_scene)
        {
            std::cout<<"Could not load model : "<<filename<<std::endl;
            return 1;
        }

        vsg::BuildAccelerationStructureTraversal buildAccelStruct(window->device());
        loaded_scene->accept(buildAccelStruct);
        tlas = buildAccelStruct.tlas;

        lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 1.0, -5.0), vsg::dvec3(0.0, 0.5, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
    }

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

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    // create DescriptorSets and binding to bind our TopLevelAcceleration structure, storage image and camra matrix uniforms
    auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{tlas}, 0, 0);

    auto storageImageDescriptor = vsg::DescriptorImageView::create(storageImageData, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    auto raytracingUniformDescriptor = vsg::DescriptorBuffer::create(raytracingUniform, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    raytracingUniformDescriptor->copyDataListToBuffers();

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorSetLayout }, vsg::PushConstantRanges{});
    auto raytracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStages, shaderGroups);
    auto bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(raytracingPipeline);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{ accelDescriptor, storageImageDescriptor, raytracingUniformDescriptor });
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

    // set up commandGraph to rendering viewport
    auto commandGraph = vsg::CommandGraph::create(window);

    auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(storageImageData._imageView, window);

    commandGraph->addChild(scenegraph);
    commandGraph->addChild(copyImageViewToWindow);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

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

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
