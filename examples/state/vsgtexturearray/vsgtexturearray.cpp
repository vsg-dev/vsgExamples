#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

void updateBaseTexture(vsg::ubvec4Array2D& image, float value)
{
    for (uint32_t r = 0; r < image.height(); ++r)
    {
        float r_ratio = static_cast<float>(r) / static_cast<float>(image.height() - 1);
        for (uint32_t c = 0; c < image.width(); ++c)
        {
            float c_ratio = static_cast<float>(c) / static_cast<float>(image.width() - 1);

            vsg::vec2 delta((r_ratio - 0.5f), (c_ratio - 0.5f));

            float angle = atan2(delta.x, delta.y);

            float distance_from_center = vsg::length(delta);

            float intensity = (sin(1.0f * angle + 30.0f * distance_from_center + 10.0f * value) + 1.0f) * 0.5f;
            image.set(c, r, vsg::ubvec4(uint8_t(intensity * intensity * 255.0f), uint8_t(intensity * 255.0f), uint8_t(intensity * 255.0f), 255));
        }
    }
    image.dirty();
}

void updateElevation(vsg::floatArray2D& heightField, float value)
{
    for (uint32_t r = 0; r < heightField.height(); ++r)
    {
        float r_ratio = static_cast<float>(r) / static_cast<float>(heightField.height() - 1);
        for (uint32_t c = 0; c < heightField.width(); ++c)
        {
            float c_ratio = static_cast<float>(c) / static_cast<float>(heightField.width() - 1);

            vsg::vec2 delta((r_ratio - 0.5f), (c_ratio - 0.5f));

            float angle = atan2(delta.x, delta.y);

            float distance_from_center = vsg::length(delta);

            float intensity = (sin(1.0f * angle + 10.0f * distance_from_center + 2.0f * value) + 1.0f) * 0.5f;
            heightField.set(c, r, intensity);
        }
    }
    heightField.dirty();
}

vsg::ref_ptr<vsg::Node> createGeometry()
{
    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {{-0.5f, -0.5f, 0.0f},
         {0.5f, -0.5f, 0.0f},
         {0.5f, 0.5f, 0.0f},
         {-0.5f, 0.5f, 0.0f}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(vertices->size(), vsg::vec3(1.0f, 1.0f, 1.0f));

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto vid = vsg::VertexIndexDraw::create();
    vid->assignArrays(vsg::DataList{vertices, colors, texcoords});
    vid->assignIndices(indices);
    vid->indexCount = 6;
    vid->instanceCount = 1;
    return vid;
}

vsg::ref_ptr<vsg::Node> createGeometry(uint32_t numColumns, uint32_t numRows)
{
    uint32_t numVertices = numColumns * numRows;
    float width = 1.0;
    float height = 1.0;

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(numVertices);
    auto texcoords = vsg::vec2Array::create(numVertices);
    auto colors = vsg::vec3Array::create(numVertices, vsg::vec3(1.0f, 1.0f, 1.0f));

    vsg::vec3 vertex_origin(0.0f, 0.0f, 0.0f);
    vsg::vec3 vertex_dx(width / (static_cast<float>(numColumns) - 1.0f), 0.0f, 0.0f);
    vsg::vec3 vertex_dy(0.0f, height / (static_cast<float>(numRows) - 1.0f), 0.0f);

    vsg::vec2 texcoord_origin(0.0f, 0.0f);
    vsg::vec2 texcoord_dx(1.0f / (static_cast<float>(numColumns) - 1.0f), 0.0f);
    vsg::vec2 texcoord_dy(0.0f, 1.0f / (static_cast<float>(numRows) - 1.0f));

    auto vertex_itr = vertices->begin();
    auto texcoord_itr = texcoords->begin();

    for (uint32_t r = 0; r < numRows; ++r)
    {
        for (uint32_t c = 0; c < numColumns; ++c)
        {
            *(vertex_itr++) = vertex_origin + vertex_dx * (static_cast<float>(c)) + vertex_dy * (static_cast<float>(r));
            *(texcoord_itr++) = texcoord_origin + texcoord_dx * (static_cast<float>(c)) + texcoord_dy * (static_cast<float>(r));
        }
    }

    uint32_t numIndices = (numColumns - 1) * (numRows - 1) * 6;
    auto indices = vsg::ushortArray::create(numIndices);
    auto itr = indices->begin();
    for (uint32_t r = 0; r < (numRows - 1); ++r)
    {
        for (uint32_t c = 0; c < (numColumns - 1); ++c)
        {
            uint32_t i = r * numColumns + c;
            // lower left triangle
            *(itr++) = i;
            *(itr++) = i + 1;
            *(itr++) = i + numColumns;

            // upper right triangle
            *(itr++) = i + numColumns;
            *(itr++) = i + 1;
            *(itr++) = i + numColumns + 1;
        }
    }

    auto vid = vsg::VertexIndexDraw::create();
    vid->assignArrays(vsg::DataList{vertices, colors, texcoords});
    vid->assignIndices(indices);
    vid->indexCount = numIndices;
    vid->instanceCount = 1;

    // vsg::write(vid, "geometry.vsgt");

    return vid;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);

    bool update = arguments.read("--update");
    int numRows = arguments.value(4, "--rows");
    int numColumns = arguments.value(4, "--cols");
    ;

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto options = vsg::Options::create();
    options->paths = searchPaths;
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    // load shaders

    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/texturearray.vert", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/texturearray.frag", options);

    if (!vertexShader || !fragmentShader)
    {
        vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/texturearray_vert.spv", searchPaths));
        fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/texturearray_frag.spv", searchPaths));
    }

    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return 1;
    }

    uint32_t numTiles = numRows * numColumns;

    vsg::ShaderStage::SpecializationConstants specializationContexts{
        {0, vsg::uintValue::create(numTiles)}, // numTiles
    };

    vertexShader->specializationConstants = specializationContexts;
    fragmentShader->specializationConstants = specializationContexts;

    // compile section
    vsg::ShaderStages stagesToCompile;
    if (vertexShader && vertexShader->module && vertexShader->module->code.empty()) stagesToCompile.emplace_back(vertexShader);
    if (fragmentShader && fragmentShader->module && fragmentShader->module->code.empty()) stagesToCompile.emplace_back(fragmentShader);

    // set up texture image
    std::vector<vsg::ref_ptr<vsg::ubvec4Array2D>> textureDataList;
    for (uint32_t i = 0; i < numTiles; ++i)
    {
        auto textureData = vsg::ubvec4Array2D::create(256, 256);
        textureData->properties.format = VK_FORMAT_R8G8B8A8_SRGB;
        if (update) textureData->properties.dataVariance = vsg::DYNAMIC_DATA;

        updateBaseTexture(*textureData, 1.0f);

        textureDataList.push_back(textureData);
    }

    // set up height fields
    std::vector<vsg::ref_ptr<vsg::floatArray2D>> heightFieldDataList;
    for (uint32_t i = 0; i < numTiles; ++i)
    {
        auto heightField = vsg::floatArray2D::create(64, 64);
        heightField->properties.format = VK_FORMAT_R32_SFLOAT;

        updateElevation(*heightField, 1.0f);
        heightFieldDataList.push_back(heightField);
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numTiles, VK_SHADER_STAGE_VERTEX_BIT, nullptr},  // { binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers}
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numTiles, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::DescriptorSetLayoutBindings tileSettingsDescriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto tileSettingsDescriptorSetLayout = vsg::DescriptorSetLayout::create(tileSettingsDescriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()};

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout, tileSettingsDescriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto clampToEdge_sampler = vsg::Sampler::create();
    clampToEdge_sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    clampToEdge_sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    vsg::ImageInfoList baseTextures;
    for (auto textureData : textureDataList)
    {
        baseTextures.push_back(vsg::ImageInfo::create(clampToEdge_sampler, textureData));
    }

    auto hf_sampler = vsg::Sampler::create();
    hf_sampler->minFilter = VK_FILTER_NEAREST;
    hf_sampler->magFilter = VK_FILTER_NEAREST;
    hf_sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    hf_sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    hf_sampler->anisotropyEnable = VK_FALSE;
    hf_sampler->maxAnisotropy = 1;
    hf_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    vsg::ImageInfoList hfTextures;
    for (auto hfData : heightFieldDataList)
    {
        hfTextures.push_back(vsg::ImageInfo::create(hf_sampler, hfData));
    }

    auto heightFieldDescriptorImage = vsg::DescriptorImage::create(hfTextures, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto baseDescriptorImage = vsg::DescriptorImage::create(baseTextures, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{heightFieldDescriptorImage, baseDescriptorImage});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSet);

    auto geometry = createGeometry(64, 64);

    for (int r = 0; r < numRows; ++r)
    {
        for (int c = 0; c < numColumns; ++c)
        {
            vsg::dvec3 position(static_cast<float>(c), static_cast<float>(r), 0.0f);

            // set up model transformation node
            auto transform = vsg::MatrixTransform::create(vsg::translate(position));

            uint32_t tileIndex = r * numColumns + c;

            auto uniformValue = vsg::uintValue::create(tileIndex);
            auto uniformBuffer = vsg::DescriptorBuffer::create(uniformValue, 0);

            auto uniformDescriptorSet = vsg::DescriptorSet::create(tileSettingsDescriptorSetLayout, vsg::Descriptors{uniformBuffer});
            auto uniformBindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, uniformDescriptorSet);

#if 1
            // assign the tileIndex uniform directly to the transform group before the geometry
            transform->addChild(uniformBindDescriptorSet);

            // add geometry
            transform->addChild(geometry);

            // add transform to root of the scene graph
            scenegraph->addChild(transform);
#else

            // use a StateGroup to assign the tileIndex
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
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // camera related details
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, radius * 1.5), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // add event handlers
    viewer->addEventHandler(vsg::Trackball::create(camera));
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // compile the Vulkan objects
    viewer->compile();

    // texture has been filled in so it's now safe to get the ImageData that holds the handles to the textures

    // create a context to manage the DeviceMemoryPool for us when we need to copy data to a staging buffer
    vsg::Context context(window->getOrCreateDevice());

    // main frame loop
    while (viewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        if (update)
        {
            // animate the transform
            float time = std::chrono::duration<float, std::chrono::seconds::period>(viewer->getFrameStamp()->time - viewer->start_point()).count();

            uint32_t textureToUpdate = 0; // viewer->getFrameStamp()->frameCount % numTiles;
            auto& textureData = textureDataList[textureToUpdate];
            if (textureData)
            {
                // update texture data
                updateBaseTexture(*textureData, time);
            }
        }

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
