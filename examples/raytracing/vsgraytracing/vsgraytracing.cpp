#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

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
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up search paths to SPIRV shaders and textures
        vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

        auto options = vsg::Options::create();
        options->paths = searchPaths;
    #ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
    #endif

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgraytracing";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        auto numFrames = arguments.value(-1, "-f");

        vsg::Path filename;
        if (argc > 1) filename = arguments[1];

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        windowTraits->imageAvailableSemaphoreWaitFlag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // enable the transfer bit as we want to copy the raytraced image to swapchain
        windowTraits->vulkanVersion = VK_API_VERSION_1_1;
        windowTraits->deviceExtensionNames = {
            VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
        };

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // load shaders

        const uint32_t shaderIndexRaygen = 0;
        const uint32_t shaderIndexMiss = 1;
        const uint32_t shaderIndexClosestHit = 2;

#if 1
        auto raygenShader = vsg::read_cast<vsg::ShaderStage>("shaders/simple_raygen.rgen", options);
        auto missShader = vsg::read_cast<vsg::ShaderStage>("shaders/simple_miss.rmiss", options);
        auto closesthitShader = vsg::read_cast<vsg::ShaderStage>("shaders/simple_closesthit.rchit", options);
#else
        vsg::ref_ptr<vsg::ShaderStage> raygenShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", vsg::findFile("shaders/simple_raygen.spv", searchPaths));
        vsg::ref_ptr<vsg::ShaderStage> missShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", vsg::findFile("shaders/simple_miss.spv", searchPaths));
        vsg::ref_ptr<vsg::ShaderStage> closesthitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", vsg::findFile("shaders/simple_closesthit.spv", searchPaths));
#endif

        if (!raygenShader || !missShader || !closesthitShader)
        {
            std::cout << "Could not create shaders." << std::endl;
            return 1;
        }

        auto shaderStages = vsg::ShaderStages{raygenShader, missShader, closesthitShader};

        // set up shader groups
        auto raygenShaderGroup = vsg::RayTracingShaderGroup::create();
        raygenShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        raygenShaderGroup->generalShader = 0;

        auto missShaderGroup = vsg::RayTracingShaderGroup::create();
        missShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        missShaderGroup->generalShader = 1;

        auto closestHitShaderGroup = vsg::RayTracingShaderGroup::create();
        closestHitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        closestHitShaderGroup->closestHitShader = 2;

        auto shaderGroups = vsg::RayTracingShaderGroups{raygenShaderGroup, missShaderGroup, closestHitShaderGroup};

        // create camera matrices and uniform for shader
        auto perspective = vsg::Perspective::create(60.0, static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height), 0.1, 10.0);
        vsg::ref_ptr<vsg::LookAt> lookAt;

        vsg::ref_ptr<vsg::Device> device;
        try
        {
            device = window->getOrCreateDevice();
        }
        catch (const vsg::Exception& exception)
        {
            std::cout << exception.message << " VkResult = " << exception.result << std::endl;
            return 0;
        }

        vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;
        if (filename.empty())
        {
            // acceleration structures
            // set up vertex and index arrays
            auto vertices = vsg::vec3Array::create(
                {{-1.0f, -1.0f, 0.0f},
                {1.0f, -1.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}});

            auto indices = vsg::uintArray::create(
                {0, 1, 2});

            // create acceleration geometry
            auto accelGeometry = vsg::AccelerationGeometry::create();
            accelGeometry->verts = vertices;
            accelGeometry->indices = indices;

            // create bottom level acceleration structure using accel geom
            auto blas = vsg::BottomLevelAccelerationStructure::create(device);
            blas->geometries.push_back(accelGeometry);

            // create top level acceleration structure
            tlas = vsg::TopLevelAccelerationStructure::create(device);

            // add geometry instance to top level acceleration structure that uses the bottom level structure
            auto geominstance = vsg::GeometryInstance::create();
            geominstance->accelerationStructure = blas;
            geominstance->transform = vsg::mat4();

            tlas->geometryInstances.push_back(geominstance);

            lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 0.0, -2.5), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
        }
        else
        {
            auto loaded_scene = vsg::read_cast<vsg::Node>(filename, options);
            if (!loaded_scene)
            {
                std::cout << "Could not load model : " << filename << std::endl;
                return 1;
            }

            vsg::BuildAccelerationStructureTraversal buildAccelStruct(device);
            loaded_scene->accept(buildAccelStruct);
            tlas = buildAccelStruct.tlas;

            lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 1.0, -5.0), vsg::dvec3(0.0, 0.5, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
        }

        // for convenience create a compile context for creating our storage image
        vsg::CompileTraversal compile(window);

        // create storage image to render into
        auto storageImage = vsg::Image::create();
        storageImage->imageType = VK_IMAGE_TYPE_2D;
        storageImage->format = VK_FORMAT_B8G8R8A8_UNORM; //VK_FORMAT_R8G8B8A8_UNORM;
        storageImage->extent.width = windowTraits->width;
        storageImage->extent.height = windowTraits->height;
        storageImage->extent.depth = 1;
        storageImage->mipLevels = 1;
        storageImage->arrayLayers = 1;
        storageImage->samples = VK_SAMPLE_COUNT_1_BIT;
        storageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
        storageImage->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        storageImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        storageImage->flags = 0;
        storageImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vsg::ImageInfo storageImageInfo{nullptr,
                                        createImageView(compile.context, storageImage, VK_IMAGE_ASPECT_COLOR_BIT),
                                        VK_IMAGE_LAYOUT_GENERAL};

        auto raytracingUniformValues = new RayTracingUniformValue();
        perspective->get_inverse(raytracingUniformValues->value().projInverse);
        lookAt->get_inverse(raytracingUniformValues->value().viewInverse);

        vsg::ref_ptr<RayTracingUniformValue> raytracingUniform(raytracingUniformValues);

        // set up graphics pipeline
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}, // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
            {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}};

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

        // create DescriptorSets and binding to bind our TopLevelAcceleration structure, storage image and camra matrix uniforms
        auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{tlas}, 0, 0);

        auto storageImageDescriptor = vsg::DescriptorImage::create(storageImageInfo, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        auto raytracingUniformDescriptor = vsg::DescriptorBuffer::create(raytracingUniform, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        raytracingUniformDescriptor->copyDataListToBuffers();

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        auto raytracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStages, shaderGroups);
        auto bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(raytracingPipeline);

        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{accelDescriptor, storageImageDescriptor, raytracingUniformDescriptor});
        auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, raytracingPipeline->getPipelineLayout(), 0, vsg::DescriptorSets{descriptorSet});

        // state group to bind the pipeline and descriptorset
        auto scenegraph = vsg::Commands::create();
        scenegraph->addChild(bindRayTracingPipeline);
        scenegraph->addChild(bindDescriptorSets);

        // setup tracing of rays
        auto traceRays = vsg::TraceRays::create();
        traceRays->raygen = raygenShaderGroup;
        traceRays->missShader = missShaderGroup;
        traceRays->hitShader = closestHitShaderGroup;
        traceRays->width = windowTraits->width;
        traceRays->height = windowTraits->height;
        traceRays->depth = 1;

        scenegraph->addChild(traceRays);

        // camera related details
        auto viewport = vsg::ViewportState::create(0, 0, windowTraits->width, windowTraits->height);
        auto camera = vsg::Camera::create(perspective, lookAt, viewport);

        // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
        viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

        // set up commandGraph to rendering viewport
        auto commandGraph = vsg::CommandGraph::create(window);

        auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(storageImageInfo.imageView, window);

        commandGraph->addChild(scenegraph);
        commandGraph->addChild(copyImageViewToWindow);

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->addEventHandler(vsg::Trackball::create(camera));

        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            //update camera matrix
            lookAt->get_inverse(raytracingUniformValues->value().viewInverse);
            raytracingUniformDescriptor->copyDataListToBuffers();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }

        // clean up done automatically thanks to ref_ptr<>
        return 0;
    }
    catch (const vsg::Exception& exception)
    {
        std::cout << exception.message << " VkResult = " << exception.result << std::endl;
        return 0;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;

}
