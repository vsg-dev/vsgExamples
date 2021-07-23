#include "Builder.h"

void Builder::setup(vsg::ref_ptr<vsg::Window> window, vsg::ViewportState* viewport, uint32_t maxNumTextures)
{
    auto device = window->getOrCreateDevice();

    _compile = vsg::CompileTraversal::create(window, viewport);

    // for now just allocated enough room for s
    uint32_t maxSets = maxNumTextures;
    vsg::DescriptorPoolSizes descriptorPoolSizes{
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxNumTextures}};

    _compile->context.descriptorPool = vsg::DescriptorPool::create(device, maxSets, descriptorPoolSizes);

    _allocatedTextureCount = 0;
    _maxNumTextures = maxNumTextures;

}

vsg::ref_ptr<vsg::BindDescriptorSets> Builder::_createTexture(const GeometryInfo& info)
{
    auto textureData = info.image;
    if (!textureData)
    {
        if (auto itr = _colorData.find(info.color); itr != _colorData.end())
        {
            textureData = itr->second;
        }
        else
        {
            auto image = _colorData[info.color] = vsg::vec4Array2D::create(2, 2, info.color, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
            image->set(0, 0, {0.0f, 1.0f, 1.0f, 1.0f});
            image->set(1, 1, {0.0f, 0.0f, 1.0f, 1.0f});
            textureData = image;
        }
    }

    auto& bindDescriptorSets = _textureDescriptorSets[textureData];
    if (bindDescriptorSets) return bindDescriptorSets;

    auto sampler = vsg::Sampler::create();

    // create texture image and associated DescriptorSets and binding
    auto texture = vsg::DescriptorImage::create(sampler, textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto descriptorSet = vsg::DescriptorSet::create(_descriptorSetLayout, vsg::Descriptors{texture});

    bindDescriptorSets = _textureDescriptorSets[textureData] = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, vsg::DescriptorSets{descriptorSet});
    return bindDescriptorSets;
}

vsg::ref_ptr<vsg::BindGraphicsPipeline> Builder::_createGraphicsPipeline()
{
    if (_bindGraphicsPipeline) return _bindGraphicsPipeline;

    std::cout<<"Builder::_initGraphicsPipeline()"<<std::endl;

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    _descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::DescriptorSetLayouts descriptorSetLayouts{_descriptorSetLayout};

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    _pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec4), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},    // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},       // tex coord data
    };


    auto rasteriszationState = vsg::RasterizationState::create();
    rasteriszationState->cullMode = VK_CULL_MODE_BACK_BIT;
    //rasteriszationState->cullMode = VK_CULL_MODE_NONE;

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        rasteriszationState,
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()};

    auto graphicsPipeline = vsg::GraphicsPipeline::create(_pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    _bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    return _bindGraphicsPipeline;
}

void Builder::compile(vsg::ref_ptr<vsg::Node> subgraph)
{
    if (verbose) std::cout << "Builder::compile(" << subgraph << ") _compile = " << _compile << std::endl;

    if (_compile)
    {
        subgraph->accept(*_compile);
        _compile->context.record();
        _compile->context.waitForCompletion();
    }
}

vsg::vec3 Builder::y_texcoord(const GeometryInfo& info) const
{
    if (info.image && info.image->getLayout().origin==vsg::Origin::TOP_LEFT)
    {
        return {1.0f, -1.0f, 0.0f};
    }
    else
    {
        return {0.0f, 1.0f, 1.0f};
    }
}

vsg::ref_ptr<vsg::Node> Builder::createBox(const GeometryInfo& info)
{
    auto& subgraph = _boxes[info];
    if (subgraph)
    {
        std::cout<<"reused createBox()"<<std::endl;
        return subgraph;
    }

    std::cout<<"new createBox()"<<std::endl;

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(_createGraphicsPipeline());
    scenegraph->add(_createTexture(info));

    auto dx = info.dx;
    auto dy = info.dy;
    auto dz = info.dz;
    auto origin = info.position - dx * 0.5f - dy * 0.5f - dz * 0.5f;
    auto [t_origin, t_scale, t_top] = y_texcoord(info).value;

    vsg::vec3 v000(origin);
    vsg::vec3 v100(origin + dx);
    vsg::vec3 v110(origin + dx + dy);
    vsg::vec3 v010(origin + dy);
    vsg::vec3 v001(origin + dz);
    vsg::vec3 v101(origin + dx + dz);
    vsg::vec3 v111(origin + dx + dy + dz);
    vsg::vec3 v011(origin + dy + dz);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {v000, v100, v101, v001,
         v100, v110, v111, v101,
         v110, v010, v011, v111,
         v010, v000, v001, v011,
         v010, v110, v100, v000,
         v001, v101, v111, v011}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(vertices->size(), vsg::vec3(1.0f, 1.0f, 1.0f));
    // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

#if 0
    vsg::vec3 n0(0.0f, -1.0f, 0.0f);
    vsg::vec3 n1(1.0f, 0.0f, 0.0f);
    vsg::vec3 n2(0.0f, 1.0f, 0.0f);
    vsg::vec3 n3(0.0f, -1.0f, 0.0f);
    vsg::vec3 n4(0.0f, 0.0f, -1.0f);
    vsg::vec3 n5(0.0f, 0.0f, 1.0f);
    auto normals = vsg::vec3Array::create(
    {
        n0, n0, n0, n0,
        n1, n1, n1, n1,
        n2, n2, n2, n2,
        n3, n3, n3, n3,
        n4, n4, n4, n4,
        n5, n5, n5, n5,
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE
#endif

    vsg::vec2 t00(0.0f, t_origin);
    vsg::vec2 t01(0.0f, t_top);
    vsg::vec2 t10(1.0f, t_origin);
    vsg::vec2 t11(1.0f, t_top);

    auto texcoords = vsg::vec2Array::create(
        {t00, t10, t11, t01,
         t00, t10, t11, t01,
         t00, t10, t11, t01,
         t00, t10, t11, t01,
         t00, t10, t11, t01,
         t00, t10, t11, t01}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2, 0, 2, 3,
         4, 5, 6, 4, 6, 7,
         8, 9, 10, 8, 10, 11,
         12, 13, 14, 12, 14, 15,
         16, 17, 18, 16, 18, 19,
         20, 21, 22, 20, 22, 23}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto vid = vsg::VertexIndexDraw::create();
    vid->arrays = vsg::DataList{vertices, colors, texcoords};
    vid->indices = indices;
    vid->indexCount = indices->size();
    vid->instanceCount = 1;

    scenegraph->addChild(vid);

    compile(scenegraph);

    subgraph = scenegraph;
    return subgraph;
}

vsg::ref_ptr<vsg::Node> Builder::createCapsule(const GeometryInfo& info)
{
    std::cout<<"createCapsule()"<<std::endl;
    return createBox(info);
}

vsg::ref_ptr<vsg::Node> Builder::createCone(const GeometryInfo& info)
{
    auto& subgraph = _cones[info];
    if (subgraph)
    {
        std::cout<<"reused createCone()"<<std::endl;
        return subgraph;
    }

    std::cout<<"new createCone()"<<std::endl;

    auto [t_origin, t_scale, t_top] = y_texcoord(info).value;

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(_createGraphicsPipeline());
    scenegraph->add(_createTexture(info));

    auto dx = info.dx * 0.5f;
    auto dy = info.dy * 0.5f;
    auto dz = info.dz * 0.5f;

    auto bottom = info.position - dz;
    auto top = info.position + dz;

    bool withEnds = false;

    unsigned int num_columns = 20;
    unsigned int num_vertices = num_columns * 2;
    unsigned int num_indices = (num_columns-1) * 3;

    if (withEnds)
    {
        num_vertices += num_columns;
        num_indices += (num_columns-2) * 3;
    }

    auto vertices = vsg::vec3Array::create(num_vertices);
    auto normals = vsg::vec3Array::create(num_vertices);
    auto texcoords = vsg::vec2Array::create(num_vertices);
    auto colors = vsg::vec3Array::create(vertices->size(), vsg::vec3(1.0f, 1.0f, 1.0f));
    auto indices = vsg::ushortArray::create(num_indices);

    vsg::vec3 v = dy;
    vsg::vec3 n = normalize(dy);
    vertices->set(0, bottom + v);
    normals->set(0, n);
    texcoords->set(0, vsg::vec2(0.0, t_origin));
    vertices->set(num_columns*2-2, bottom+v);
    normals->set(num_columns*2-2, n);
    texcoords->set(num_columns*2-2, vsg::vec2(1.0, t_origin));

    vertices->set(1, top);
    normals->set(1, n);
    texcoords->set(1, vsg::vec2(0.0, t_top));
    vertices->set(num_columns*2-1, top);
    normals->set(num_columns*2-1, n);
    texcoords->set(num_columns*2-1, vsg::vec2(1.0, t_top));

    for(unsigned int c = 1; c < num_columns-1; ++c)
    {
        unsigned int vi = c*2;
        float r = float(c)/float(num_columns-1);
        float alpha = (r) * 2.0 * vsg::PI;
        v = dx * (-sinf(alpha)) + dy * (cosf(alpha));

        float alpha_neighbour = alpha+0.01;
        vsg::vec3 v_neighour = dx * (-sinf(alpha_neighbour)) + dy * (cosf(alpha_neighbour));
        vsg::vec3 delta = v_neighour - v ;

        n = vsg::normalize( vsg::cross(v-top, v_neighour - v) );

        vertices->set(vi, bottom + v);
        normals->set(vi, n);
        texcoords->set(vi, vsg::vec2(r, t_origin));

        vertices->set(vi+1, top);
        normals->set(vi+1, n);
        texcoords->set(vi+1, vsg::vec2(r, t_top));
    }

    unsigned int i = 0;
    for(unsigned int c = 0; c < num_columns-1; ++c)
    {
        unsigned lower = c*2;
        unsigned upper = lower + 1;

        indices->set(i++, lower);
        indices->set(i++, lower + 2);
        indices->set(i++, upper);
    }

    if (withEnds)
    {
        unsigned int bottom_i = num_columns*2;
        v = dy;
        vsg::vec3 bottom_n = normalize(-dz);

        vertices->set(bottom_i, bottom + v);
        normals->set(bottom_i, bottom_n);
        texcoords->set(bottom_i, vsg::vec2(0.0, t_origin));
        vertices->set(bottom_i + num_columns - 1 , bottom + v);
        normals->set(bottom_i + num_columns - 1, bottom_n);
        texcoords->set(bottom_i + num_columns - 1, vsg::vec2(1.0, t_origin));

        for(unsigned int c = 1; c < num_columns-1; ++c)
        {
            float r = float(c)/float(num_columns-1);
            float alpha = (r) * 2.0 * vsg::PI;
            v = dx * (-sinf(alpha)) + dy * (cosf(alpha));

            unsigned int vi = bottom_i + c;
            vertices->set(vi, bottom + v);
            normals->set(vi, bottom_n);
            texcoords->set(vi, vsg::vec2(r, t_origin));
        }

        for(unsigned int c = 0; c < num_columns-2; ++c)
        {
            indices->set(i++, bottom_i + c);
            indices->set(i++, bottom_i + num_columns - 1);
            indices->set(i++, bottom_i + c + 1);
        }
    }

    // setup geometry
    auto vid = vsg::VertexIndexDraw::create();
    vid->arrays = vsg::DataList{vertices, colors, texcoords};
    vid->indices = indices;
    vid->indexCount = indices->size();
    vid->instanceCount = 1;

    scenegraph->addChild(vid);

    compile(scenegraph);

    subgraph = scenegraph;
    return subgraph;
}

vsg::ref_ptr<vsg::Node> Builder::createCylinder(const GeometryInfo& info)
{
    auto& subgraph = _cylinders[info];
    if (subgraph)
    {
        std::cout<<"reused createCylinder()"<<std::endl;
        return subgraph;
    }

    std::cout<<"new createCylinder()"<<std::endl;

    auto [t_origin, t_scale, t_top] = y_texcoord(info).value;

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(_createGraphicsPipeline());
    scenegraph->add(_createTexture(info));

    auto dx = info.dx * 0.5f;
    auto dy = info.dy * 0.5f;
    auto dz = info.dz * 0.5f;

    auto bottom = info.position - dz;
    auto top = info.position + dz;

    bool withEnds = true;

    unsigned int num_columns = 20;
    unsigned int num_vertices = num_columns * 2;
    unsigned int num_indices = (num_columns-1) * 6;

    if (withEnds)
    {
        num_vertices += num_columns * 2;
        num_indices += (num_columns-2) * 6;
    }

    auto vertices = vsg::vec3Array::create(num_vertices);
    auto normals = vsg::vec3Array::create(num_vertices);
    auto texcoords = vsg::vec2Array::create(num_vertices);
    auto colors = vsg::vec3Array::create(vertices->size(), vsg::vec3(1.0f, 1.0f, 1.0f));
    auto indices = vsg::ushortArray::create(num_indices);


    vsg::vec3 v = dy;
    vsg::vec3 n = normalize(dy);
    vertices->set(0, bottom + v);
    normals->set(0, n);
    texcoords->set(0, vsg::vec2(0.0, t_origin));
    vertices->set(num_columns*2-2, bottom+v);
    normals->set(num_columns*2-2, n);
    texcoords->set(num_columns*2-2, vsg::vec2(1.0, t_origin));

    vertices->set(1, top + v);
    normals->set(1, n);
    texcoords->set(1, vsg::vec2(0.0, t_top));
    vertices->set(num_columns*2-1, top+v);
    normals->set(num_columns*2-1, n);
    texcoords->set(num_columns*2-1, vsg::vec2(1.0, t_top));

    for(unsigned int c = 1; c < num_columns-1; ++c)
    {
        unsigned int vi = c*2;
        float r = float(c)/float(num_columns-1);
        float alpha = (r) * 2.0 * vsg::PI;
        v = dx * (-sinf(alpha)) + dy * (cosf(alpha));
        n = normalize(v);

        vertices->set(vi, bottom + v);
        normals->set(vi, n);
        texcoords->set(vi, vsg::vec2(r, t_origin));

        vertices->set(vi+1, top + v);
        normals->set(vi+1, n);
        texcoords->set(vi+1, vsg::vec2(r, t_top));
    }

    unsigned int i = 0;
    for(unsigned int c = 0; c < num_columns-1; ++c)
    {
        unsigned lower = c*2;
        unsigned upper = lower + 1;

        indices->set(i++, lower);
        indices->set(i++, lower + 2);
        indices->set(i++, upper);

        indices->set(i++, upper);
        indices->set(i++, lower + 2);
        indices->set(i++, upper + 2);
    }

    if (withEnds)
    {
        unsigned int bottom_i = num_columns*2;
        v = dy;
        vsg::vec3 bottom_n = normalize(-dz);
        vertices->set(bottom_i, bottom + v);
        normals->set(bottom_i, n);
        texcoords->set(bottom_i, vsg::vec2(0.0, t_origin));
        vertices->set(bottom_i + num_columns - 1 , bottom + v);
        normals->set(bottom_i + num_columns - 1, n);
        texcoords->set(bottom_i + num_columns - 1, vsg::vec2(1.0, t_origin));

        unsigned int top_i = bottom_i + num_columns;
        vsg::vec3 top_n = normalize(dz);
        vertices->set(top_i, top + v);
        normals->set(top_i, n);
        texcoords->set(top_i, vsg::vec2(0.0, t_top));
        vertices->set(top_i + num_columns - 1, top + v);
        normals->set(top_i + num_columns - 1, n);
        texcoords->set(top_i + num_columns - 1, vsg::vec2(0.0, t_top));

        for(unsigned int c = 1; c < num_columns-1; ++c)
        {
            float r = float(c)/float(num_columns-1);
            float alpha = (r) * 2.0 * vsg::PI;
            v = dx * (-sinf(alpha)) + dy * (cosf(alpha));
            n = normalize(v);

            unsigned int vi = bottom_i + c;
            vertices->set(vi, bottom + v);
            normals->set(vi, bottom_n);
            texcoords->set(vi, vsg::vec2(r, t_origin));

            vi = top_i + c;
            vertices->set(vi, top + v);
            normals->set(vi, top_n);
            texcoords->set(vi, vsg::vec2(r, t_top));
        }

        for(unsigned int c = 0; c < num_columns-2; ++c)
        {
            indices->set(i++, bottom_i + c);
            indices->set(i++, bottom_i + num_columns - 1);
            indices->set(i++, bottom_i + c + 1);
        }

        for(unsigned int c = 0; c < num_columns-2; ++c)
        {
            indices->set(i++, top_i + c);
            indices->set(i++, top_i + c + 1);
            indices->set(i++, top_i + num_columns - 1);
        }
    }

    // setup geometry
    auto vid = vsg::VertexIndexDraw::create();
    vid->arrays = vsg::DataList{vertices, colors, texcoords};
    vid->indices = indices;
    vid->indexCount = indices->size();
    vid->instanceCount = 1;

    scenegraph->addChild(vid);

    compile(scenegraph);

    subgraph = scenegraph;
    return subgraph;
}

vsg::ref_ptr<vsg::Node> Builder::createQuad(const GeometryInfo& info)
{
    auto& subgraph = _boxes[info];
    if (subgraph)
    {
        std::cout<<"reused createQuad()"<<std::endl;
        return subgraph;
    }

    std::cout<<"new createQuad()"<<std::endl;

    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(_createGraphicsPipeline());
    scenegraph->add(_createTexture(info));

    auto dx = info.dx;
    auto dy = info.dy;
    auto origin = info.position - dx * 0.5f - dy * 0.5f;
    auto [t_origin, t_scale, t_top] = y_texcoord(info).value;

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {origin,
         origin + dx,
         origin + dx + dy,
         origin + dy}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
        {{1.0f, 0.0f, 0.0f},
         {0.0f, 1.0f, 0.0f},
         {0.0f, 0.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, t_origin},
         {1.0f, t_origin},
         {1.0f, t_top},
         {0.0f, t_top}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {
            0,
            1,
            2,
            2,
            3,
            0,
        }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto vid = vsg::VertexIndexDraw::create();
    vid->arrays = vsg::DataList{vertices, colors, texcoords};
    vid->indices = indices;
    vid->indexCount = indices->size();
    vid->instanceCount = 1;

    scenegraph->addChild(vid);

    compile(scenegraph);

    return scenegraph;
}

vsg::ref_ptr<vsg::Node> Builder::createSphere(const GeometryInfo& info)
{
    auto& subgraph = _spheres[info];
    if (subgraph)
    {
        std::cout<<"reused createSphere()"<<std::endl;
        return subgraph;
    }

    std::cout<<"new createSphere()"<<std::endl;

    auto [t_origin, t_scale, t_top] = y_texcoord(info).value;

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(_createGraphicsPipeline());
    scenegraph->add(_createTexture(info));

    auto dx = info.dx * 0.5f;
    auto dy = info.dy * 0.5f;
    auto dz = info.dz * 0.5f;
    auto origin = info.position;

    unsigned int num_columns = 20;
    unsigned int num_rows = 10;
    unsigned int num_vertices = num_columns * num_rows;
    unsigned int num_indices = (num_columns-1) * (num_rows-1) * 6;

    auto vertices = vsg::vec3Array::create(num_vertices);
    auto normals = vsg::vec3Array::create(num_vertices);
    auto texcoords = vsg::vec2Array::create(num_vertices);
    auto colors = vsg::vec3Array::create(vertices->size(), vsg::vec3(1.0f, 1.0f, 1.0f));
    auto indices = vsg::ushortArray::create(num_indices);

    for(unsigned int r = 0; r < num_rows; ++r)
    {
        float beta = ((float(r)/float(num_rows-1)) - 0.5) * vsg::PI;
        float ty = t_origin + t_scale * float(r)/float(num_rows-1);
        float cos_beta = cosf(beta);
        vsg::vec3 dz_sin_beta = dz * sinf(beta);

        vsg::vec3 v = dy * cos_beta + dz_sin_beta;
        vsg::vec3 n = normalize(v);

        unsigned int left_i = r * num_columns;
        vertices->set(left_i, v + origin);
        normals->set(left_i, n);
        texcoords->set(left_i, vsg::vec2(0.0f, ty));

        unsigned int right_i = left_i + num_columns-1;
        vertices->set(right_i, v + origin);
        normals->set(right_i, n);
        texcoords->set(right_i, vsg::vec2(1.0f, ty));

        for(unsigned int c = 1; c < num_columns-1; ++c)
        {
            unsigned int vi = left_i + c;
            float alpha = (float(c)/float(num_columns-1)) * 2.0 * vsg::PI;
            v = dx * (-sinf(alpha) * cos_beta) + dy * (cosf(alpha) * cos_beta) + dz_sin_beta;
            n = normalize(v);
            vertices->set(vi, origin + v);
            normals->set(vi, n);
            texcoords->set(vi, vsg::vec2(float(c)/float(num_columns-1), ty));
        }
    }

    unsigned int i = 0;
    for(unsigned int r = 0; r < num_rows-1; ++r)
    {
        for(unsigned int c = 0; c < num_columns-1; ++c)
        {
            unsigned lower = num_columns * r + c;
            unsigned upper = lower + num_columns;

            indices->set(i++, lower);
            indices->set(i++, lower + 1);
            indices->set(i++, upper);

            indices->set(i++, upper);
            indices->set(i++, lower + 1);
            indices->set(i++, upper + 1);
        }
    }

    // setup geometry
    auto vid = vsg::VertexIndexDraw::create();
    vid->arrays = vsg::DataList{vertices, colors, texcoords};
    vid->indices = indices;
    vid->indexCount = indices->size();
    vid->instanceCount = 1;

    scenegraph->addChild(vid);

    compile(scenegraph);

    subgraph = scenegraph;
    return subgraph;
}
