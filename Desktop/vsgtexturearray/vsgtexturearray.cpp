#include <vsg/all.h>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

#include <iostream>

void update(vsg::vec4Array2D& image, float value)
{
    for(size_t r = 0; r < image.height(); ++r)
    {
        float r_ratio = static_cast<float>(r)/static_cast<float>(image.height()-1);
        for(size_t c = 0; c < image.width(); ++c)
        {
            float c_ratio = static_cast<float>(c)/static_cast<float>(image.width()-1);

            vsg::vec2 delta((r_ratio-0.5f), (c_ratio-0.5f));

            float angle = atan2(delta.x, delta.y);

            float distance_from_center = vsg::length(delta);

            float intensity = (sin(1.0 * angle + 30.0f * distance_from_center+10.0*value) + 1.0f)*0.5f;
            image.set(c, r, vsg::vec4(intensity*intensity, intensity, intensity, 1.0f));
        }
    }
}

vsg::ref_ptr<vsg::Node> createGeometry()
{
    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
    {
        {-0.5f, -0.5f, 0.0f},
        {0.5f,  -0.5f, 0.0f},
        {0.5f , 0.5f, 0.0f},
        {-0.5f, 0.5f, 0.0f}
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(vertices->size(), vsg::vec3(1.0f, 1.0f, 1.0f));

    auto texcoords = vsg::vec2Array::create(
    {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
    {
        0, 1, 2,
        2, 3, 0
    }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto vid = vsg::VertexIndexDraw::create();
    vid->arrays = vsg::DataList{vertices, colors, texcoords};
    vid->indices = indices;
    vid->indexCount = 6;
    vid->instanceCount = 1;
    return vid;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto options = vsg::Options::create();
#ifdef USE_VSGXCHANGE
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
#endif
    options->paths = searchPaths;

    // load shaders

    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/texturearray.vert", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/texturearray.frag", options);

    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return 1;
    }

    int numRows = 4;
    int numColumns = 4;

    uint32_t numBaseTextures = numRows * numColumns;

    fragmentShader->setSpecializationConstants({
        {0, vsg::uintValue::create(numBaseTextures)}, // numBaseTextures
    });


#ifdef USE_VSGXCHANGE
    // compile section
    vsg::ShaderStages stagesToCompile;
    if (vertexShader) stagesToCompile.emplace_back(vertexShader);
    if (fragmentShader) stagesToCompile.emplace_back(fragmentShader);

    if (!stagesToCompile.empty())
    {
        auto shaderCompiler = vsgXchange::ShaderCompiler::create();

        std::vector<std::string> defines;
        shaderCompiler->compile(stagesToCompile, defines); // and paths?
    }
    // TODO end of block requiring changes
#endif

    // set up texture image
    std::vector<vsg::ref_ptr<vsg::vec4Array2D>> textureDataList;
    for(uint32_t i = 0; i<numBaseTextures; ++i)
    {
        auto textureData = vsg::vec4Array2D::create(256, 256);
        textureData->getLayout().format = VK_FORMAT_R32G32B32A32_SFLOAT;

        update(*textureData, 1.0f);

        textureDataList.push_back(textureData);
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numBaseTextures, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::DescriptorSetLayoutBindings tileSettingsDescriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto tileSettingsDescriptorSetLayout = vsg::DescriptorSetLayout::create(tileSettingsDescriptorBindings);

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
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

    vsg::GraphicsPipelineStates pipelineStates
    {
        vsg::VertexInputState::create( vertexBindingsDescriptions, vertexAttributeDescriptions ),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()
    };

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout, tileSettingsDescriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto clampToEdge_sampler = vsg::Sampler::create();
    clampToEdge_sampler->info().addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    clampToEdge_sampler->info().addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    vsg::SamplerImages baseTextures;
    for(auto textureData : textureDataList)
    {
        baseTextures.emplace_back(vsg::SamplerImage{clampToEdge_sampler, textureData});
    }

    auto baseDescriptorImage = vsg::DescriptorImage::create(baseTextures, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{baseDescriptorImage});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, descriptorSet);
    bindDescriptorSet->setSlot(1);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    auto geometry = createGeometry();


    for(int r=0; r<numRows; ++r)
    {
        for(int c=0; c<numColumns; ++c)
        {
            vsg::dvec3 position(static_cast<float>(c), static_cast<float>(r), 0.0f);

            // set up model transformation node
            auto transform = vsg::MatrixTransform::create(vsg::translate(position));

            uint32_t tileIndex = r*numColumns + c;

            auto uniformValue = vsg::uintValue::create(tileIndex);
            auto uniformBuffer = vsg::DescriptorBuffer::create(uniformValue, 0);

            auto uniformDscriptorSet = vsg::DescriptorSet::create(tileSettingsDescriptorSetLayout, vsg::Descriptors{uniformBuffer});
            bindDescriptorSet->setSlot(2);

            auto uniformBindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, uniformDscriptorSet);

#if 0
            transform->addChild(uniformBindDescriptorSet);

            // add geometry
            transform->addChild(geometry);

            // add transform to root of the scene graph
            scenegraph->addChild(transform);
#else

            auto tileSettingsGroup = vsg::StateGroup::create();
            tileSettingsGroup->add(uniformBindDescriptorSet);
            tileSettingsGroup->addChild(transform);

            transform->addChild(geometry);

            // add transform to root of the scene graph
            scenegraph->addChild(tileSettingsGroup);
#endif
        }
    }

    // vsg::write(scenegraph, "test.vsgt");

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.001;

    // camera related details
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 4.5);
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, radius*1.5), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);

    auto copyCmd = vsg::CopyAndReleaseImageDataCommand::create();
    commandGraph->getChildren().insert(commandGraph->getChildren().begin(), copyCmd);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // add event handlers
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // compile the Vulkan objects
    viewer->compile();

    // texture has been filled in so it's now safe to get the ImageData that holds the handles to the texture's
    vsg::ImageData textureImageData;
    if (!baseDescriptorImage->getImageList(0).empty()) textureImageData = baseDescriptorImage->getImageList(0)[0]; // contextID=0, and only one imageData

    // create a context to manage the DeviceMemoryPool for us when we need to copy data to a staging buffer
    vsg::Context context(window->getOrCreateDevice());

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        // animate the transform
        float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();

        auto textureData = textureDataList[0];

        // update texture data
        update(*textureData, time);

        // transfer data to staging buffer
        auto stagingBufferData = vsg::copyDataToStagingBuffer(context, textureData);

        // schedule a copy command to do the staging buffer to the texture image, this copy command is recorded to the appropriate command buffer by viewer->recordAndSubmit().
        copyCmd->add(stagingBufferData, textureImageData);

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
