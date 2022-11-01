#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

int main(int argc, char** argv)
{
    auto options = vsg::Options::create();

    // set up search paths to shaders
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    windowTraits->synchronizationLayer = arguments.read("--sync");
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // load shaders
    auto computeShader = vsg::read_cast<vsg::ShaderStage>("shaders/computevertex.comp", options);
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/computevertex.vert", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/computevertex.frag", options);

    if (!computeShader || !vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return 1;
    }

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    auto device = window->getOrCreateDevice();

    VkDeviceSize bufferSize = sizeof(vsg::vec4) * 256;
    auto buffer = vsg::createBufferAndMemory(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto bufferInfo = vsg::BufferInfo::create();
    bufferInfo->buffer = buffer;
    bufferInfo->offset = 0;
    bufferInfo->range = bufferSize;

    // camera related details
    auto viewport = vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height);
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    // setting we pass to the compute shader each frame to provide animation of vertex data.
    auto computeScale = vsg::vec4Array::create(1);
    computeScale->properties.dataVariance = vsg::DYNAMIC_DATA;
    computeScale->set(0, vsg::vec4(1.0, 1.0, 1.0, 0.0));

    auto computeScaleBuffer = vsg::DescriptorBuffer::create(computeScale, 1);

    // create the compute graph to compute the positions of the vertices
    auto physicalDevice = window->getOrCreatePhysicalDevice();
    auto computeQueueFamily = physicalDevice->getQueueFamily(VK_QUEUE_COMPUTE_BIT);
    auto computeCommandGraph = vsg::CommandGraph::create(device, computeQueueFamily);
    {
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr} // ClipSettings uniform
        };

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create( vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        computeCommandGraph->addChild(bindPipeline);

        auto stroageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{bufferInfo}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        auto descriptorSet = vsg::DescriptorSet::create( descriptorSetLayout, vsg::Descriptors{stroageBuffer, computeScaleBuffer});
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

        computeCommandGraph->addChild(bindDescriptorSet);

        computeCommandGraph->addChild(vsg::Dispatch::create(1, 1, 1));
    }

    // set up graphics subgraph to render the computed vertices
    auto graphicCommandGraph = vsg::CommandGraph::create(window);
    {
        // set up graphics pipeline
        vsg::PushConstantRanges pushConstantRanges{
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls automatically provided by the VSG's DispatchTraversal
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

        // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
        auto scenegraph = vsg::StateGroup::create();
        scenegraph->add(bindGraphicsPipeline);

        auto renderGraph = vsg::createRenderGraphForView(window, camera, scenegraph);
        graphicCommandGraph->addChild(renderGraph);

        // setup geometry
        auto drawCommands = vsg::Commands::create();
        auto bind_vertex_buffer = vsg::BindVertexBuffers::create();
        bind_vertex_buffer->arrays.push_back(bufferInfo);
        drawCommands->addChild(bind_vertex_buffer);
        drawCommands->addChild(vsg::Draw::create(256, 1, 0, 0));

        scenegraph->addChild(drawCommands);
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);
    // viewer->assignRecordAndSubmitTaskAndPresentation({graphicCommandGraph, computeCommandGraph});
    viewer->assignRecordAndSubmitTaskAndPresentation({computeCommandGraph, graphicCommandGraph});

    // compile the Vulkan objects
    viewer->compile();

    viewer->addEventHandler(vsg::Trackball::create(camera));

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        double frameTime = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
        computeScale->at(0).z = sin(frameTime);
        computeScale->dirty();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
