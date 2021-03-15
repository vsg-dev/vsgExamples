#include "TileReader.h"

vsg::dbox TileReader::computeTileExtents(uint32_t x, uint32_t y, uint32_t level) const
{
    double multiplier = pow(0.5, double(level));
    double tileWidth = multiplier * (extents.max.x - extents.min.x) / double(noX);
    double tileHeight = multiplier * (extents.max.y - extents.min.y) / double(noY);

    vsg::dbox tile_extents;
    if (originTopLeft)
    {
        tile_extents.min = vsg::dvec3(double(x) * tileWidth, -double(y + 1) * tileHeight, 0.0);
        tile_extents.max = vsg::dvec3(double(x + 1) * tileWidth, -double(y) * tileHeight, 1.0);
    }
    else
    {
        tile_extents.min = vsg::dvec3(double(x) * tileWidth, double(y) * tileHeight, 0.0);
        tile_extents.max = vsg::dvec3(double(x + 1) * tileWidth, double(y + 1) * tileHeight, 1.0);
    }
    return tile_extents;
}

vsg::Path TileReader::getTilePath(const vsg::Path& src, uint32_t x, uint32_t y, uint32_t level) const
{
    auto replace = [](vsg::Path& path, const std::string& match, uint32_t value) {
        std::stringstream sstr;
        sstr << value;
        auto levelPos = path.find(match);
        if (levelPos != std::string::npos) path.replace(levelPos, match.length(), sstr.str());
    };

    vsg::Path path = src;
    replace(path, "{z}", level);
    replace(path, "{x}", x);
    replace(path, "{y}", y);

    return path;
}

vsg::ref_ptr<vsg::Object> TileReader::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    auto extension = vsg::fileExtension(filename);
    if (extension != "tile") return {};

    std::string tile_info = filename.substr(0, filename.length() - 5);
    if (tile_info == "root")
    {
        return read_root(options);
    }
    else
    {
        std::stringstream sstr(tile_info);

        uint32_t x, y, lod;
        sstr >> x >> y >> lod;

        // std::cout<<"read("<<filename<<") -> tile_info = "<<tile_info<<", x = "<<x<<", y = "<<y<<", z = "<<lod<<std::endl;

        return read_subtile(x, y, lod, options);
    }
}

vsg::ref_ptr<vsg::Object> TileReader::read_root(vsg::ref_ptr<const vsg::Options> options) const
{
    auto group = createRoot();

    uint32_t lod = 0;
    for (uint32_t y = 0; y < noY; ++y)
    {
        for (uint32_t x = 0; x < noX; ++x)
        {
            auto imagePath = getTilePath(imageLayer, x, y, lod);
            //auto terrainPath = getTilePath(terrainLayer, x, y, lod);

            auto imageTile = vsg::read_cast<vsg::Data>(imagePath, options);
            //auto terrainTile = vsg::read(terrainPath, options);

            if (imageTile)
            {
                auto tile_extents = computeTileExtents(x, y, lod);
                auto tile = createTextureQuad(tile_extents, imageTile, mipmapLevelsHint);

                if (tile)
                {
                    auto plod = vsg::PagedLOD::create();
                    plod->setBound(vsg::dsphere(0.0, 0.0, 0.0, 180.0));
                    plod->setChild(0, vsg::PagedLOD::Child{0.25, {}});  // external child visible when it's bound occupies more than 1/4 of the height of the window
                    plod->setChild(1, vsg::PagedLOD::Child{0.0, tile}); // visible always
                    plod->filename = vsg::make_string(x, " ", y, " 0.tile");
                    plod->options = options;

                    group->addChild(plod);
                }
            }
        }
    }

    uint32_t maxLevel = 20;
    uint32_t estimatedNumOfTilesBelow = 0;
    uint32_t maxNumTilesBelow = 40000;

    uint32_t level = 0;
    for (uint32_t i = level; i < maxLevel; ++i)
    {
        estimatedNumOfTilesBelow += std::pow(4, i - level);
    }

    uint32_t tileMultiplier = std::min(estimatedNumOfTilesBelow, maxNumTilesBelow) + 1;

    vsg::CollectDescriptorStats collectStats;
    group->accept(collectStats);

    auto resourceHints = vsg::ResourceHints::create();

    resourceHints->maxSlot = collectStats.maxSlot;
    resourceHints->numDescriptorSets = static_cast<uint32_t>(collectStats.computeNumDescriptorSets() * tileMultiplier);
    resourceHints->descriptorPoolSizes = collectStats.computeDescriptorPoolSizes();

    for (auto& poolSize : resourceHints->descriptorPoolSizes)
    {
        poolSize.descriptorCount = poolSize.descriptorCount * tileMultiplier;
    }

    group->setObject("ResourceHints", resourceHints);
    return group;
}

vsg::ref_ptr<vsg::Object> TileReader::read_subtile(uint32_t x, uint32_t y, uint32_t lod, vsg::ref_ptr<const vsg::Options> options) const
{
    // std::cout<<"Need to load subtile for "<<x<<", "<<y<<", "<<lod<<std::endl;

    // need to load subtile x y lod

    auto group = vsg::Group::create();

    uint32_t subtile_x = x * 2;
    uint32_t subtile_y = y * 2;
    uint32_t local_lod = lod + 1;
    for (uint32_t dy = 0; dy < 2; ++dy)
    {
        for (uint32_t dx = 0; dx < 2; ++dx)
        {
            uint32_t local_x = subtile_x + dx;
            uint32_t local_y = subtile_y + dy;

            auto imagePath = getTilePath(imageLayer, local_x, local_y, local_lod);
            auto imageTile = vsg::read_cast<vsg::Data>(imagePath, options);

            // std::cout<<"    local_x = "<<local_x<<", local_y = "<<local_y<<", local_lod = "<<local_lod<<", imagePath =  "<<imagePath<<std::endl;

            if (imageTile)
            {
                auto tile_extents = computeTileExtents(local_x, local_y, local_lod);
                auto tile = createTextureQuad(tile_extents, imageTile, mipmapLevelsHint);

                if (tile)
                {
                    auto plod = vsg::PagedLOD::create();
                    plod->setBound(vsg::dsphere(0.0, 0.0, 0.0, 180.0));
                    plod->setChild(0, vsg::PagedLOD::Child{0.25, {}});  // external child visible when it's bound occupies more than 1/4 of the height of the window
                    plod->setChild(1, vsg::PagedLOD::Child{0.0, tile}); // visible always
                    plod->filename = vsg::make_string(local_x, " ", local_y, " ", local_lod, ".tile");
                    plod->options = options;

                    //std::cout<<"plod->filename "<<plod->filename<<std::endl;

                    group->addChild(plod);
                }
            }
            else
            {
                // std::cout<<"Failed to read subtile "<<local_x<<", "<<local_y<<", "<<local_lod<<std::endl;
            }
        }
    }

    if (group->getNumChildren() != 4)
    {
        std::cout << "Warning: could not load all 4 subtiles, loaded only " << group->getNumChildren() << std::endl;

        return {};
    }

    return group;
}

void TileReader::init()
{
    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
}

vsg::ref_ptr<vsg::StateGroup> TileReader::createRoot() const
{
    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return {};
    }

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

    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    auto root = vsg::StateGroup::create();
    root->add(bindGraphicsPipeline);

    return root;
}

vsg::ref_ptr<vsg::Node> TileReader::createTextureQuad(const vsg::dbox& extents, vsg::ref_ptr<vsg::Data> textureData, uint32_t mipmapLevelsHint) const
{
    if (!textureData) return {};

    // create texture image and associated DescriptorSets and binding
    auto sampler = vsg::Sampler::create();
    sampler->maxLod = mipmapLevelsHint;
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    auto texture = vsg::DescriptorImage::create(sampler, textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup to bind any texture state
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    float min_x = extents.min.x;
    float min_y = extents.min.y;
#if 1
    float max_x = extents.max.x;
    float max_y = extents.max.y;
#else
    float max_x = extents.min.x * 0.05 + extents.max.x * 0.95;
    float max_y = extents.min.y * 0.05 + extents.max.y * 0.95;
#endif

    auto vertices = vsg::vec3Array::create(
        {{min_x, 0.0f, min_y},
         {max_x, 0.0f, min_y},
         {max_x, 0.0f, max_y},
         {min_x, 0.0f, max_y}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
        {{1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f}}); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    uint8_t origin = textureData->getLayout().origin; // in Vulkan the origin is by default top left.
    float left = 0.0f;
    float right = 1.0f;
    float top = (origin == vsg::TOP_LEFT) ? 0.0f : 1.0f;
    float bottom = (origin == vsg::TOP_LEFT) ? 1.0f : 0.0f;
    auto texcoords = vsg::vec2Array::create(
        {{left, bottom},
         {right, bottom},
         {right, top},
         {left, top}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    return scenegraph;
}
