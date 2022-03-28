#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

vsg::ref_ptr<vsg::ShaderSet> createPhongShaderSet(vsg::ref_ptr<vsg::Options> options)
{
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp.vert", options);
    //auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_flat_shaded.frag", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/assimp_phong.frag", options);
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord0", "", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 3, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    shaderSet->addUniformBinding("displacementMap", "VSG_DISPLACEMENT_MAP", 0, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Array2D::create(1,1));
    shaderSet->addUniformBinding("diffuseMap", "VSG_DIFFUSE_MAP", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1,1));
    shaderSet->addUniformBinding("normalMap", "VSG_NORMAL_MAP", 0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1,1));
    shaderSet->addUniformBinding("aoMap", "VSG_LIGHTMAP_MAP", 0, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1,1));
    shaderSet->addUniformBinding("emissiveMap", "VSG_EMISSIVE_MAP", 0, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1,1));
    shaderSet->addUniformBinding("material", "", 0, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create());
    shaderSet->addUniformBinding("lightData", "VSG_VIEW_LIGHT_DATA", 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    return shaderSet;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->objectCache = vsg::ObjectCache::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);

    auto textureFile = arguments.value<vsg::Path>("", "-t");
    auto shaderSetFile = arguments.value<vsg::Path>("", "-s");
    auto outputFile = arguments.value<vsg::Path>("", "-o");
    auto outputShaderSetFile = arguments.value<vsg::Path>("", "--os");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    vsg::ref_ptr<vsg::ShaderSet> shaderSet;
    if (!shaderSetFile.empty())
    {
        shaderSet = vsg::read_cast<vsg::ShaderSet>(shaderSetFile, options);
        std::cout<<"Read ShaderSet file "<<shaderSet<<std::endl;
    }

    // no ShaderSet loaded so fallback to create function.
    if (!shaderSet) shaderSet = createPhongShaderSet(options);

    if (!shaderSet)
    {
        std::cout << "Could not create shaders." << std::endl;
        return 1;
    }

    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfig::create(shaderSet);

    // set up graphics pipeline
    vsg::Descriptors descriptors;

    // read texture image
    if (!textureFile.empty())
    {
        auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
        if (!textureData)
        {
            std::cout << "Could not read texture file : " << textureFile << std::endl;
            return 1;
        }

        // enable texturing
        graphicsPipelineConfig->assignTexture(descriptors, "diffuseMap", textureData);
    }

    // set up pass of material
    auto mat = vsg::PhongMaterialValue::create();
    mat->value().diffuse.set(1.0f, 1.0f, 0.0f, 1.0f);
    mat->value().specular.set(1.0f, 0.0f, 0.0f, 1.0f);

    graphicsPipelineConfig->assignUniform(descriptors, "material", mat);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {{-0.5f, -0.5f, 0.0f},
         {0.5f, -0.5f, 0.0f},
         {0.5f, 0.5f, 0.0f},
         {-0.5f, 0.5f, 0.0f},
         {-0.5f, -0.5f, -0.5f},
         {0.5f, -0.5f, -0.5f},
         {0.5f, 0.5f, -0.5f},
         {-0.5f, 0.5f, -0.5f}});

    auto normals = vsg::vec3Array::create(
        {{0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f}});

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f},
         {0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}});

    auto colors = vsg::vec4Array::create(
        {{1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f},
        });

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0,
         4, 5, 6,
         6, 7, 4});

    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normals);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, colors);

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));

    // set up the DescriptorSetLayout, PipelineLayout and GraphicsPipeline
    graphicsPipelineConfig->init();

    auto descriptorSet = vsg::DescriptorSet::create(graphicsPipelineConfig->descriptorSetLayout, descriptors);
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineConfig->layout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(graphicsPipelineConfig->bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create();

    // add transform to root of the scene graph
    scenegraph->addChild(transform);


    // add drawCommands to transform
    transform->addChild(drawCommands);


    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // camera related details
    auto viewport = vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height);
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    if (!outputShaderSetFile.empty())
    {
        vsg::write(shaderSet, outputShaderSetFile, options);
        return 0;
    }

    if (!outputFile.empty())
    {
        vsg::write(scenegraph, outputFile, options);
        return 0;
    }

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    viewer->addEventHandler(vsg::Trackball::create(camera));

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        // animate the transform
        float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();
        transform->matrix = vsg::rotate(time * vsg::radians(90.0f), vsg::vec3(0.0f, 0.0, 1.0f));

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
