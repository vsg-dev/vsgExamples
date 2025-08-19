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

vsg::ref_ptr<vsg::Node> createLoadedModelScene(vsg::CommandLine& arguments, vsg::ref_ptr<vsg::Options> options)
{
    auto group = vsg::Group::create();

    vsg::Path path;

    // read any vsg files
    for (int i = 1; i < arguments.argc(); ++i)
    {
        vsg::Path filename = arguments[i];
        path = vsg::filePath(filename);

        auto object = vsg::read(filename, options);
        if (auto node = object.cast<vsg::Node>())
        {
            group->addChild(node);
        }
        else if (object)
        {
                std::cout << "Unable to view object of type " << object->className() << std::endl;
        }
        else
        {
            std::cout << "Unable to load file " << filename << std::endl;
        }
    }

    if (group->children.empty())
        return {};
    else if (group->children.size() == 1)
        return group->children[0];
    else
        return group;
}

template<typename T>
T random_in_range(T min, T max)
{
    return min + static_cast<T>(std::rand()) / static_cast<T>(RAND_MAX) * (max-min);
}

vsg::ref_ptr<vsg::Node> createComputeScene(vsg::CommandLine& /*arguments*/, vsg::ref_ptr<vsg::Options> options)
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
    auto computeShader = vsg::read_cast<vsg::ShaderStage>("shaders/drawindirect.comp", options);
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/drawindirect.vert", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/drawindirect.frag", options);

    if (!computeShader || !vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }


#if 1
    VkDeviceSize bufferSize = sizeof(vsg::vec4) * 256;
    auto buffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto bufferInfo = vsg::BufferInfo::create();
    bufferInfo->buffer = buffer;
    bufferInfo->offset = 0;
    bufferInfo->range = bufferSize;
#else
    auto positions = vsg::vec4Array::create(256);
    for(auto& v : *positions) v.set(random_in_range(-0.5f, 0.5f), random_in_range(-0.5f, 0.5f), random_in_range(0.0f, 1.0f), 1.0f);
    auto bufferInfo = vsg::BufferInfo::create(positions);
#endif

    // compute subgraph
    {

        auto computeScale = vsg::vec4Array::create(1);
        computeScale->properties.dataVariance = vsg::DYNAMIC_DATA;
        computeScale->set(0, vsg::vec4(1.0, 1.0, 1.0, 0.0));

        auto computeScaleBuffer = vsg::DescriptorBuffer::create(computeScale, 1);

        auto computeCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily);
        computeCommandGraph->submitOrder = -1; // make sure the computeCommandGraph is placed before first

        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr} // ClipSettings uniform
        };

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        computeCommandGraph->addChild(bindPipeline);

        auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{bufferInfo}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer, computeScaleBuffer});
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

        computeCommandGraph->addChild(bindDescriptorSet);

        computeCommandGraph->addChild(vsg::Dispatch::create(4, 4, 1));

        auto postComputeBarrier = vsg::BufferMemoryBarrier::create(VK_ACCESS_SHADER_WRITE_BIT,
                                                                   VK_ACCESS_SHADER_READ_BIT,
                                                                   VK_QUEUE_FAMILY_IGNORED,
                                                                   VK_QUEUE_FAMILY_IGNORED,
                                                                   buffer,
                                                                   0,
                                                                   bufferSize);


        auto postComputeBarrierCmd = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, postComputeBarrier);

        group->addChild(computeCommandGraph);
    }

    // set up graphics subgraph to render the computed vertices
    {
        // set up graphics pipeline
        vsg::PushConstantRanges pushConstantRanges{
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
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
        auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
        auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

        // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
        auto scenegraph = vsg::StateGroup::create();
        scenegraph->add(bindGraphicsPipeline);

        // setup geometry
        auto drawCommands = vsg::Commands::create();
        auto bind_vertex_buffer = vsg::BindVertexBuffers::create();
        bind_vertex_buffer->arrays.push_back(bufferInfo);
        drawCommands->addChild(bind_vertex_buffer);


        auto drawIndirectCommandArray = vsg::DrawIndirectCommandArray::create(1);

        auto& drawIndirectCommand = drawIndirectCommandArray->at(0);
        drawIndirectCommand.vertexCount = 256;
        drawIndirectCommand.instanceCount = 1;
        drawIndirectCommand.firstVertex = 0;
        drawIndirectCommand.firstInstance = 0;

        auto drawIndirectBufferInfo = vsg::BufferInfo::create(drawIndirectCommandArray);

        auto drawIndirect = vsg::DrawIndirect::create();
        drawIndirect->bufferInfo = drawIndirectBufferInfo;
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

vsg::ref_ptr<vsg::Node> createScene(vsg::CommandLine& arguments, vsg::ref_ptr<vsg::Options> options)
{
    if (arguments.read("-m"))
    {
        return createLoadedModelScene(arguments, options);
    }
    else
    {
        return createComputeScene(arguments, options);
    }
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
