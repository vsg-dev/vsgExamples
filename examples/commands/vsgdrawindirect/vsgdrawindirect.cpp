#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

// background reading:
// GLSL compute shaders: https://www.khronos.org/opengl/wiki/Compute_Shader
// NVIidia's Vulkan do's and dont's: https://developer.nvidia.com/blog/vulkan-dos-donts/
// https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/
// https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html
// https://docs.vulkan.org/guide/latest/extensions/VK_KHR_synchronization2.html
// https://cpp-rendering.io/barriers-vulkan-not-difficult/

template<typename T>
T random_in_range(T min, T max)
{
    return min + static_cast<T>(std::rand()) / static_cast<T>(RAND_MAX) * (max-min);
}

class CaptureViewState : public vsg::Inherit<vsg::Group, CaptureViewState>
{
public:

    CaptureViewState() {};

    void accept(vsg::RecordTraversal& rt) const override
    {
        auto state = rt.getState();
        vsg::info("\nCaptureViewState::accept()");
        vsg::info("projection = ", state->projectionMatrixStack.top());
        vsg::info("modelview = ", state->modelviewMatrixStack.top());
        auto pvm = state->projectionMatrixStack.top() * state->modelviewMatrixStack.top();
        vsg::info("pvm = ", pvm);

        vsg::dplane left(pvm[0][0] + pvm[0][3], pvm[1][0] + pvm[1][3], pvm[2][0] + pvm[2][3], pvm[3][0] + pvm[3][3]);
        vsg::dplane right(-pvm[0][0] + pvm[0][3], -pvm[1][0] + pvm[1][3], -pvm[2][0] + pvm[2][3], -pvm[3][0] + pvm[3][3]);
        vsg::dplane bottom(pvm[0][1] + pvm[0][3], pvm[1][1] + pvm[1][3], pvm[2][1] + pvm[2][3], pvm[3][1] + pvm[3][3]);
        vsg::dplane top(-pvm[0][1] + pvm[0][3], -pvm[1][1] + pvm[1][3], -pvm[2][1] + pvm[2][3], -pvm[3][1] + pvm[3][3]);

        left.vec /= vsg::length(left.n);
        right.vec /= vsg::length(right.n);
        bottom.vec /= vsg::length(bottom.n);
        top.vec /= vsg::length(top.n);

        vsg::info("left = ", left);
        vsg::info("right = ", right);
        vsg::info("bottom = ", bottom);
        vsg::info("top = ", top);
        rt.apply(*this);
    }
};

vsg::ref_ptr<vsg::Node> createScene(vsg::CommandLine& /*arguments*/, vsg::ref_ptr<vsg::Options> options)
{
    auto group = vsg::Group::create();

    auto device = options->getRefObject<vsg::Device>("device");
    if (!device)
    {
        std::cout<<"Cannot allocate GPU memory for compute shader wihtout vsg::Device."<<std::endl;
    }

    int computeQueueFamily = 0;
    if (!options->getValue("computeQueueFamily", computeQueueFamily))
    {
        std::cout<<"Cannot setup compute command buffer without computeQueueFamily."<<std::endl;
    }

    // load shaders
    auto init_computeShader = vsg::read_cast<vsg::ShaderStage>("shaders/drawindirect_initialize.comp", options);
    auto computeShader = vsg::read_cast<vsg::ShaderStage>("shaders/drawindirect.comp", options);
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/drawindirect.vert", options);
    auto geometryShader = vsg::read_cast<vsg::ShaderStage>("shaders/drawindirect.geom", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/drawindirect.frag", options);


    if (!init_computeShader || !computeShader || !vertexShader || !geometryShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

    bool generateDebugInfo = false;
    if (options->getValue("generateDebugInfo", generateDebugInfo))
    {
        auto hints = vsg::ShaderCompileSettings::create();
        hints->generateDebugInfo = generateDebugInfo;

        computeShader->module->hints = hints;
        init_computeShader->module->hints = hints;
        vertexShader->module->hints = hints;
        geometryShader->module->hints = hints;
        fragmentShader->module->hints = hints;
    }

    VkDeviceSize vertex_bufferSize = sizeof(vsg::vec4) * 256;
    auto vertex_buffer = vsg::createBufferAndMemory(device, vertex_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto vertex_bufferInfo = vsg::BufferInfo::create();
    vertex_bufferInfo->buffer = vertex_buffer;
    vertex_bufferInfo->offset = 0;
    vertex_bufferInfo->range = vertex_bufferSize;


    VkDeviceSize drawIndirect_bufferSize = sizeof(vsg::DrawIndirectCommand);
    auto drawIndirect_buffer = vsg::createBufferAndMemory(device, drawIndirect_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto drawIndirect_bufferInfo = vsg::BufferInfo::create();
    drawIndirect_bufferInfo->buffer = drawIndirect_buffer;
    drawIndirect_bufferInfo->offset = 0;
    drawIndirect_bufferInfo->range = drawIndirect_bufferSize;

    // compute subgraph
    {
        auto computeScale = vsg::vec4Array::create(1);
        computeScale->properties.dataVariance = vsg::DYNAMIC_DATA;
        computeScale->set(0, vsg::vec4(1.0, 1.0, 1.0, 1.0));

        auto vertexStorageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{vertex_bufferInfo}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        auto computeScaleBuffer = vsg::DescriptorBuffer::create(computeScale, 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        auto drawIndirectStorageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{drawIndirect_bufferInfo}, 2, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        auto computeCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily);
        computeCommandGraph->submitOrder = -1; // make sure the computeCommandGraph is placed before parent CommandGraph

        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
        };

        vsg::PushConstantRanges pushConstantRanges{
            {VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
        };

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{vertexStorageBuffer, computeScaleBuffer, drawIndirectStorageBuffer});
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

#if 1
        auto captureViewState = CaptureViewState::create();
        computeCommandGraph->addChild(captureViewState);
        auto parent = captureViewState;
#else
        auto parent = computeCommandGraph;
#endif

        // initialize drawIndirect buffer
        {
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, init_computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

            auto stateGroup = vsg::StateGroup::create();
            stateGroup->add(bindPipeline);
            stateGroup->add(bindDescriptorSet);
            stateGroup->addChild(vsg::Dispatch::create(1, 1, 1));

            parent->addChild(stateGroup);
        }

        {
            auto postInitComputeBarrier = vsg::BufferMemoryBarrier::create(VK_ACCESS_SHADER_WRITE_BIT,
                                                                            VK_ACCESS_SHADER_READ_BIT,
                                                                            VK_QUEUE_FAMILY_IGNORED,
                                                                            VK_QUEUE_FAMILY_IGNORED,
                                                                            drawIndirect_buffer,
                                                                            0,
                                                                            drawIndirect_bufferSize);

            auto postInitComputeBarrierCmd = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, postInitComputeBarrier);

            parent->addChild(postInitComputeBarrierCmd);
        }

        // update vertex and drawIndirect buffers
        {
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

            auto stateGroup = vsg::StateGroup::create();
            stateGroup->add(bindPipeline);
            stateGroup->add(bindDescriptorSet);
            stateGroup->addChild(vsg::Dispatch::create(4, 4, 1));

            parent->addChild(stateGroup);
        }

        {
            auto postComputeBarrier = vsg::BufferMemoryBarrier::create(VK_ACCESS_SHADER_WRITE_BIT,
                                                                        VK_ACCESS_SHADER_READ_BIT,
                                                                        VK_QUEUE_FAMILY_IGNORED,
                                                                        VK_QUEUE_FAMILY_IGNORED,
                                                                        vertex_buffer,
                                                                        0,
                                                                        vertex_bufferSize);


            auto postComputeBarrierCmd = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, postComputeBarrier);

            parent->addChild(postComputeBarrierCmd);
        }

        group->addChild(computeCommandGraph);
    }

    // set up graphics subgraph to render the computed vertices
    {
        // set up graphics pipeline
        vsg::PushConstantRanges pushConstantRanges{
            {VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
        };

        vsg::VertexInputState::Bindings vertexBindingsDescriptions{
            VkVertexInputBindingDescription{0, sizeof(vsg::vec4), VK_VERTEX_INPUT_RATE_VERTEX}};

        vsg::VertexInputState::Attributes vertexAttributeDescriptions{
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0}};

        auto inputAssemblyState = vsg::InputAssemblyState::create();
        inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

        vsg::GraphicsPipelineStates pipelineStates{
            vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
            inputAssemblyState,
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()};

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{}, pushConstantRanges);
        auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, geometryShader, fragmentShader}, pipelineStates);
        auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

        // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
        auto scenegraph = vsg::StateGroup::create();
        scenegraph->add(bindGraphicsPipeline);

        // setup geometry
        auto drawCommands = vsg::Commands::create();
        auto bind_vertex_buffer = vsg::BindVertexBuffers::create();
        bind_vertex_buffer->arrays.push_back(vertex_bufferInfo);
        drawCommands->addChild(bind_vertex_buffer);



        auto drawIndirect = vsg::DrawIndirect::create();
        drawIndirect->bufferInfo = drawIndirect_bufferInfo;
        drawIndirect->drawCount = 1;
        drawIndirect->stride = 16;

        drawCommands->addChild(drawIndirect);

        scenegraph->addChild(drawCommands);

        group->addChild(scenegraph);
    }

#if 1
    // create base
    {
        auto builder = vsg::Builder::create();
        builder->options = options;

        vsg::GeometryInfo geom;
        geom.dz.set(0.0f, 0.0f, 0.05f);

        vsg::StateInfo state;
        state.image = vsg::read_cast<vsg::Data>("textures/lz.vsgb", options);

        group->addChild(builder->createBox(geom, state));
    }
#endif


    return group;
}

void enableGenerateDebugInfo(vsg::ref_ptr<vsg::Options> options)
{
    auto shaderHints = vsg::ShaderCompileSettings::create();
    shaderHints->generateDebugInfo = true;

    auto& text = options->shaderSets["text"] = vsg::createTextShaderSet(options);
    text->defaultShaderHints = shaderHints;

    auto& flat = options->shaderSets["flat"] = vsg::createFlatShadedShaderSet(options);
    flat->defaultShaderHints = shaderHints;

    auto& phong = options->shaderSets["phong"] = vsg::createPhongShaderSet(options);
    phong->defaultShaderHints = shaderHints;

    auto& pbr = options->shaderSets["pbr"] = vsg::createPhysicsBasedRenderingShaderSet(options);
    pbr->defaultShaderHints = shaderHints;
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // create windowTraits using the any command line arugments to configure settings
        auto windowTraits = vsg::WindowTraits::create(arguments);

        // enable the use of compute shader when we create the vsg::Device which will be done at Window creation time
        windowTraits->queueFlags |= VK_QUEUE_COMPUTE_BIT;

        // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

        bool reportAverageFrameRate = arguments.read("--fps");
        if (arguments.read({"-t", "--test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->fullscreen = true;
            reportAverageFrameRate = true;
        }
        if (arguments.read({"--st", "--small-test"}))
        {
            windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            windowTraits->width = 192, windowTraits->height = 108;
            windowTraits->decoration = false;
            reportAverageFrameRate = true;
        }

        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);
        auto logFilename = arguments.value<vsg::Path>("", "--log");

        vsg::ref_ptr<vsg::Instrumentation> instrumentation;
        if (arguments.read({"--gpu-annotation", "--ga"}) && vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            windowTraits->debugUtils = true;

            auto gpu_instrumentation = vsg::GpuAnnotation::create();
            if (arguments.read("--name"))
                gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_name;
            else if (arguments.read("--className"))
                gpu_instrumentation->labelType = vsg::GpuAnnotation::Object_className;
            else if (arguments.read("--func"))
                gpu_instrumentation->labelType = vsg::GpuAnnotation::SourceLocation_function;

            instrumentation = gpu_instrumentation;
        }
        else if (arguments.read({"--profiler", "--pr"}))
        {
            // set Profiler options
            auto settings = vsg::Profiler::Settings::create();
            arguments.read("--cpu", settings->cpu_instrumentation_level);
            arguments.read("--gpu", settings->gpu_instrumentation_level);
            arguments.read("--log-size", settings->log_size);

            // create the profiler
            instrumentation = vsg::Profiler::create(settings);
        }

        if (arguments.read({"--shader-debug-info", "--sdi"}))
        {
            options->setValue("generateDebugInfo", true);
            windowTraits->deviceExtensionNames.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
        }

        // enable geometry shader and other option Vulkan features.
        auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;
        deviceFeatures->get().geometryShader = VK_TRUE;

        vsg::Path outputFilename;
        arguments.read<vsg::Path>("-o", outputFilename);

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

        auto commandGraph = vsg::CommandGraph::create(window);

        auto numFrames = arguments.value(-1, "-f");
        auto nearFarRatio = arguments.value<double>(0.001, "--nfr");

        auto device = window->getOrCreateDevice();

        options->setObject("device", device);
        options->setValue("computeQueueFamily", commandGraph->queueFamily);

        auto vsg_scene = createScene(arguments, options);

        if (!vsg_scene)
        {
            std::cout << "Please specify a 3d model on the command line." << std::endl;
            return 1;
        }

        if (outputFilename)
        {
            vsg::write(vsg_scene, outputFilename, options);

            return 0;
        }


        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);

        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.5);
        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        viewer->addEventHandler(vsg::Trackball::create(camera));

        commandGraph->addChild(vsg::createRenderGraphForView(window, camera, vsg_scene));

        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        if (instrumentation) viewer->assignInstrumentation(instrumentation);

        viewer->compile();

        viewer->start_point() = vsg::clock::now();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();
            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();
        }

        if (auto profiler = instrumentation.cast<vsg::Profiler>())
        {
            instrumentation->finish();
            if (logFilename)
            {
                std::ofstream fout(logFilename);
                profiler->log->report(fout);
            }
            else
            {
                profiler->log->report(std::cout);
            }
        }

        if (reportAverageFrameRate)
        {
            auto fs = viewer->getFrameStamp();
            double fps = static_cast<double>(fs->frameCount) / std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - viewer->start_point()).count();
            std::cout << "Average frame rate = " << fps << " fps" << std::endl;
        }
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
