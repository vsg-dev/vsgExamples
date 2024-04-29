#include "TileReader.h"

vsg::dvec3 TileReader::computeLatitudeLongitudeAltitude(const vsg::dvec3& src) const
{
    if (projection == "EPSG:3857" || projection == "spherical-mercator")
    {
        double n = 2.0 * vsg::radians(src.y);
        double adjustedLatitude = vsg::degrees(atan(0.5 * (exp(n) - exp(-n))));
        return vsg::dvec3(adjustedLatitude, src.x, src.z);
    }
    else
    {
        return vsg::dvec3(src.y, src.x, src.z);
    }
}

vsg::dbox TileReader::computeTileExtents(uint32_t x, uint32_t y, uint32_t level) const
{
    double multiplier = pow(0.5, double(level));
    double tileWidth = multiplier * (extents.max.x - extents.min.x) / double(noX);
    double tileHeight = multiplier * (extents.max.y - extents.min.y) / double(noY);

    vsg::dbox tile_extents;
    if (originTopLeft)
    {
        vsg::dvec3 origin(extents.min.x, extents.max.y, extents.min.z);
        tile_extents.min = origin + vsg::dvec3(double(x) * tileWidth, -double(y + 1) * tileHeight, 0.0);
        tile_extents.max = origin + vsg::dvec3(double(x + 1) * tileWidth, -double(y) * tileHeight, 1.0);
    }
    else
    {
        tile_extents.min = extents.min + vsg::dvec3(double(x) * tileWidth, double(y) * tileHeight, 0.0);
        tile_extents.max = extents.min + vsg::dvec3(double(x + 1) * tileWidth, double(y + 1) * tileHeight, 1.0);
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
    auto extension = vsg::lowerCaseFileExtension(filename);
    if (extension != ".tile") return {};

    auto tile_info = filename.substr(0, filename.length() - 5);
    if (tile_info == "root")
    {
        return read_root(options);
    }
    else
    {
        std::basic_stringstream<vsg::Path::value_type> sstr(tile_info.native());

        uint32_t x, y, lod;
        sstr >> x >> y >> lod;

        // std::cout<<"read("<<filename<<") -> tile_info = "<<tile_info<<", x = "<<x<<", y = "<<y<<", z = "<<lod<<std::endl;

        return read_subtile(x, y, lod, options);
    }
}

vsg::ref_ptr<vsg::Object> TileReader::read_root(vsg::ref_ptr<const vsg::Options> options) const
{
    auto group = createRoot();
    if (!group) return {};

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
                auto tile = createTile(tile_extents, imageTile);
                if (tile)
                {
                    vsg::ComputeBounds computeBound;
                    tile->accept(computeBound);
                    auto& bb = computeBound.bounds;
                    vsg::dsphere bound((bb.min.x + bb.max.x) * 0.5, (bb.min.y + bb.max.y) * 0.5, (bb.min.z + bb.max.z) * 0.5, vsg::length(bb.max - bb.min) * 0.5);

                    auto plod = vsg::PagedLOD::create();
                    plod->bound = bound;
                    plod->children[0] = vsg::PagedLOD::Child{0.25, {}};  // external child visible when it's bound occupies more than 1/4 of the height of the window
                    plod->children[1] = vsg::PagedLOD::Child{0.0, tile}; // visible always
                    plod->filename = vsg::make_string(x, " ", y, " 0.tile");
                    plod->options = vsg::Options::create_if(options, *options);

                    group->addChild(plod);
                }
            }
        }
    }

    uint32_t estimatedNumOfTilesBelow = 0;
    uint32_t maxNumTilesBelow = 1024;

    uint32_t level = 0;
    for (uint32_t i = level; i < maxLevel; ++i)
    {
        estimatedNumOfTilesBelow += static_cast<uint32_t>(std::pow(4, i - level));
    }

    uint32_t tileMultiplier = std::min(estimatedNumOfTilesBelow, maxNumTilesBelow) + 1;

    // set up the ResourceHints required to make sure the VSG preallocates enough Vulkan resources for the paged database
    vsg::CollectResourceRequirements collectRequirements;
    group->accept(collectRequirements);
    group->setObject("ResourceHints", collectRequirements.createResourceHints(tileMultiplier));

    // assign the EllipsoidModel so that the overall geometry of the database can be used as guide for clipping and navigation.
    group->setObject("EllipsoidModel", ellipsoidModel);

    return group;
}

vsg::ref_ptr<vsg::Object> TileReader::read_subtile(uint32_t x, uint32_t y, uint32_t lod, vsg::ref_ptr<const vsg::Options> options) const
{
    // std::cout<<"Need to load subtile for "<<x<<", "<<y<<", "<<lod<<std::endl;

    // need to load subtile x y lod

    vsg::time_point start_read = vsg::clock::now();

    auto group = vsg::Group::create();

    struct TileID
    {
        uint32_t local_x;
        uint32_t local_y;
    };

    vsg::Paths tiles;
    std::map<vsg::Path, TileID> pathToTileID;

    uint32_t subtile_x = x * 2;
    uint32_t subtile_y = y * 2;
    uint32_t local_lod = lod + 1;
    for (uint32_t dy = 0; dy < 2; ++dy)
    {
        for (uint32_t dx = 0; dx < 2; ++dx)
        {
            uint32_t local_x = subtile_x + dx;
            uint32_t local_y = subtile_y + dy;
            auto tilePath = getTilePath(imageLayer, local_x, local_y, local_lod);
            tiles.push_back(tilePath);
            pathToTileID[tilePath] = TileID{local_x, local_y};
        }
    }

    auto pathObjects = vsg::read(tiles, options);

    if (pathObjects.size() == 4)
    {
        for (auto& [tilePath, object] : pathObjects)
        {
            auto& tileID = pathToTileID[tilePath];
            auto imageTile = object.cast<vsg::Data>();
            if (imageTile)
            {
                auto tile_extents = computeTileExtents(tileID.local_x, tileID.local_y, local_lod);
                auto tile = createTile(tile_extents, imageTile);
                if (tile)
                {
                    vsg::ComputeBounds computeBound;
                    tile->accept(computeBound);
                    auto& bb = computeBound.bounds;
                    vsg::dsphere bound((bb.min.x + bb.max.x) * 0.5, (bb.min.y + bb.max.y) * 0.5, (bb.min.z + bb.max.z) * 0.5, vsg::length(bb.max - bb.min) * 0.5);

                    if (local_lod < maxLevel)
                    {
                        auto plod = vsg::PagedLOD::create();
                        plod->bound = bound;
                        plod->children[0] = vsg::PagedLOD::Child{lodTransitionScreenHeightRatio, {}}; // external child visible when its bound occupies more than 1/4 of the height of the window
                        plod->children[1] = vsg::PagedLOD::Child{0.0, tile};                          // visible always
                        plod->filename = vsg::make_string(tileID.local_x, " ", tileID.local_y, " ", local_lod, ".tile");
                        plod->options = vsg::Options::create_if(options, *options);

                        //std::cout<<"plod->filename "<<plod->filename<<std::endl;

                        group->addChild(plod);
                    }
                    else
                    {
                        auto cullGroup = vsg::CullGroup::create();
                        cullGroup->bound = bound;
                        cullGroup->addChild(tile);

                        group->addChild(cullGroup);
                    }
                }
            }
        }
    }

    vsg::time_point end_read = vsg::clock::now();

    double time_to_read_tile = std::chrono::duration<float, std::chrono::milliseconds::period>(end_read - start_read).count();

    {
        std::scoped_lock<std::mutex> lock(statsMutex);
        numTilesRead += 1;
        totalTimeReadingTiles += time_to_read_tile;
    }

    if (group->children.size() != 4)
    {
        vsg::warn("Warning: could not load all 4 subtiles, loaded only ", group->children.size());

        return {};
    }

    return group;
}

void TileReader::init()
{
    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers}
    };

    descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
    };

    pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);

    sampler = vsg::Sampler::create();
    sampler->maxLod = static_cast<float>(mipmapLevelsHint);
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->anisotropyEnable = VK_TRUE;
    sampler->maxAnisotropy = 16.0f;
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
        vsg::warn("Could not find shaders. Please set the VSG_FILE_PATH env var to the path to your vsgExamples/data.");
        if (!searchPaths.empty())
        {
            vsg::info("VSG_FILE_PATH set, but does not contains vert_PushConstants.spv & frag_PushConstants.spv shaders. Paths set:");
            for (auto path : searchPaths)
            {
                vsg::info("    ", path);
            }
        }
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

vsg::ref_ptr<vsg::Node> TileReader::createTile(const vsg::dbox& tile_extents, vsg::ref_ptr<vsg::Data> sourceData) const
{
#if 1
    return createECEFTile(tile_extents, sourceData);
#else
    return createTextureQuad(tile_extents, sourceData);
#endif
}

vsg::ref_ptr<vsg::Node> TileReader::createECEFTile(const vsg::dbox& tile_extents, vsg::ref_ptr<vsg::Data> textureData) const
{
    vsg::dvec3 center = computeLatitudeLongitudeAltitude((tile_extents.min + tile_extents.max) * 0.5);

    auto localToWorld = ellipsoidModel->computeLocalToWorldTransform(center);
    auto worldToLocal = vsg::inverse(localToWorld);

    // create texture image and associated DescriptorSets and binding
    auto texture = vsg::DescriptorImage::create(sampler, textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup to bind any texture state
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(localToWorld); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    uint32_t numRows = 32;
    uint32_t numCols = 32;
    uint32_t numVertices = numRows * numCols;
    uint32_t numTriangles = (numRows - 1) * (numCols - 1) * 2;

    double longitudeOrigin = tile_extents.min.x;
    double longitudeScale = (tile_extents.max.x - tile_extents.min.x) / double(numCols - 1);
    double latitudeOrigin = tile_extents.min.y;
    double latitudeScale = (tile_extents.max.y - tile_extents.min.y) / double(numRows - 1);

    float sCoordScale = 1.0f / float(numCols - 1);
    float tCoordScale = 1.0f / float(numRows - 1);
    float tCoordOrigin = 0.0;
    if (textureData->properties.origin == vsg::TOP_LEFT)
    {
        tCoordScale = -tCoordScale;
        tCoordOrigin = 1.0f;
    }

    vsg::vec3 color(1.0f, 1.0f, 1.0f);

    // set up vertex coords
    auto vertices = vsg::vec3Array::create(numVertices);
    auto colors = vsg::vec3Array::create(numVertices);
    auto texcoords = vsg::vec2Array::create(numVertices);
    for (uint32_t r = 0; r < numRows; ++r)
    {
        for (uint32_t c = 0; c < numCols; ++c)
        {
            vsg::dvec3 location(longitudeOrigin + double(c) * longitudeScale, latitudeOrigin + double(r) * latitudeScale, 0.0);
            vsg::dvec3 latitudeLongitudeAltitude = computeLatitudeLongitudeAltitude(location);

            auto ecef = ellipsoidModel->convertLatLongAltitudeToECEF(latitudeLongitudeAltitude);
            vsg::vec3 vertex(worldToLocal * ecef);
            vsg::vec2 texcoord(float(c) * sCoordScale, tCoordOrigin + float(r) * tCoordScale);

            uint32_t vi = c + r * numCols;
            vertices->set(vi, vertex);
            colors->set(vi, color);
            texcoords->set(vi, texcoord);
        }
    }

    // set up indices
    auto indices = vsg::ushortArray::create(numTriangles * 3);
    auto itr = indices->begin();
    for (uint32_t r = 0; r < numRows - 1; ++r)
    {
        for (uint32_t c = 0; c < numCols - 1; ++c)
        {
            uint32_t vi = c + r * numCols;
            (*itr++) = vi;
            (*itr++) = vi + 1;
            (*itr++) = vi + numCols;
            (*itr++) = vi + numCols;
            (*itr++) = vi + 1;
            (*itr++) = vi + numCols + 1;
        }
    }

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(indices->size(), 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    return scenegraph;
}

vsg::ref_ptr<vsg::Node> TileReader::createTextureQuad(const vsg::dbox& tile_extents, vsg::ref_ptr<vsg::Data> textureData) const
{
    if (!textureData) return {};

    // create texture image and associated DescriptorSets and binding
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
    float min_x = static_cast<float>(tile_extents.min.x);
    float min_y = static_cast<float>(tile_extents.min.y);
    float max_x = static_cast<float>(tile_extents.max.x);
    float max_y = static_cast<float>(tile_extents.max.y);

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

    uint8_t origin = textureData->properties.origin; // in Vulkan the origin is by default top left.
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
