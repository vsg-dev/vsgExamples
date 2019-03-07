#include <vsg/all.h>

#include <iostream>
#include <chrono>

vsg::ref_ptr<vsg::Node> createSceneData(vsg::Paths& searchPaths)
{
    //
    // load shaders
    //
    vsg::ref_ptr<vsg::ShaderModule> vertexShader = vsg::ShaderModule::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderModule> fragmentShader = vsg::ShaderModule::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return vsg::ref_ptr<vsg::Node>();
    }

    std::string textureFile("textures/lz.vsgb");

    //
    // set up texture image
    //
    vsg::vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile(textureFile, searchPaths));
    if (!textureData)
    {
        std::cout<<"Could not read texture file : "<<textureFile<<std::endl;
        return vsg::ref_ptr<vsg::Node>();
    }

    //
    // set up graphics pipeline
    //

    // gp->maxSets = 1;
    // gp->descriptorPoolSizes = vsg::DescriptorPoolSizes
    // {
    //     {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // texture
    // };

    vsg::DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
    vsg::DescriptorSetLayouts descriptorSetLayouts{descriptorSetLayout};

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    vsg::ShaderModules shaders{vertexShader, fragmentShader};

    vsg::GraphicsPipelineStates pipelineStates = vsg::GraphicsPipelineStates
    {
        vsg::ShaderStages::create(shaders),
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()
    };

    auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // crete StateGroup to hold the GraphicsProgram to decorate the whole command graph
    auto gp = vsg::StateGroup::create();
    gp->add(bindGraphicsPipeline);

    //
    // set up model transformation node
    //
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT, 128

    // add transform to graphics pipeline group
    gp->addChild(transform);


    //
    // create texture node
    //
    vsg::ref_ptr<vsg::Texture> texture = vsg::Texture::create();
    texture->_textureData = textureData;
    //texture->_samplerInfo =
    texture->_dstBinding = 0;

    vsg::Descriptors descriptors{texture};
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayouts, descriptors);
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, vsg::DescriptorSets{descriptorSet});

    auto textureGroup = vsg::StateGroup::create();
    textureGroup->add(bindDescriptorSets);

    // add texture node to transform node
    transform->addChild(textureGroup);

    //
    // set up vertex and index arrays
    //
    vsg::ref_ptr<vsg::vec3Array> vertices(new vsg::vec3Array
    {
        {-0.5f, -0.5f, 0.0f},
        {0.5f,  -0.5f, 0.05f},
        {0.5f , 0.5f, 0.0f},
        {-0.5f, 0.5f, 0.0f},
        {-0.5f, -0.5f, -0.5f},
        {0.5f,  -0.5f, -0.5f},
        {0.5f , 0.5f, -0.5},
        {-0.5f, 0.5f, -0.5}
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    vsg::ref_ptr<vsg::vec3Array> colors(new vsg::vec3Array
    {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    vsg::ref_ptr<vsg::vec2Array> texcoords(new vsg::vec2Array
    {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray
    {
        0, 1, 2,
        2, 3, 0,
        4, 5, 6,
        6, 7, 4
    }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto geometry = vsg::Geometry::create();

    // setup geometry
    geometry->_arrays = vsg::DataList{vertices, colors, texcoords};
    geometry->_indices = indices;

    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0);
    geometry->_commands = vsg::Geometry::Commands{drawIndexed};

    // add geometry to texture group
    textureGroup->addChild(geometry);

    return gp;
}


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto debugLayer = arguments.read({"--debug","-d"});
    auto apiDumpLayer = arguments.read({"--api","-a"});
    auto numFrames = arguments.value(-1, "-f");
    auto printFrameRate = arguments.read("--fr");
    auto numWindows = arguments.value(1, "--num-windows");
    auto [width, height] = arguments.value(std::pair<uint32_t, uint32_t>(800, 600), {"--window", "-w"});
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // create the scene/command graph
    vsg::ref_ptr<vsg::Node> commandGraph = createSceneData(searchPaths);

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(width, height, debugLayer, apiDumpLayer));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    for(int i=1; i<numWindows; ++i)
    {
        vsg::ref_ptr<vsg::Window> new_window(vsg::Window::create(width, height, debugLayer, apiDumpLayer, window));
        viewer->addWindow( new_window );
    }

    // camera related state
    auto viewport = vsg::ViewportState::create(VkExtent2D{width, height});
    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 10.0));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, viewport));

    // create graphics stage
    auto stage = vsg::GraphicsStage::create(commandGraph, camera);

    // add a GraphicsStage to the Window to do dispatch of the command graph to the commnad buffer(s)
    window->addStage(stage);

    // compile the Vulkan objects
    viewer->compile();

    auto startTime =std::chrono::steady_clock::now();

    // assign a Trackball and CloseHandler to the Viewer to respond to events
    auto trackball = vsg::Trackball::create(camera);
    viewer->addEventHandlers({trackball, vsg::CloseHandler::create(viewer)});

    bool windowResized = false;
    float time = 0.0f;

    while (viewer->active() && (numFrames<0 || (numFrames--)>0))
    {
        // poll events and advance frame counters
        viewer->advance();

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        float previousTime = time;
        time = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::steady_clock::now()-viewer->start_point()).count();
        if (printFrameRate) std::cout<<"time = "<<time<<" fps="<<1.0/(time-previousTime)<<std::endl;

        if (viewer->aquireNextFrame())
        {
            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
