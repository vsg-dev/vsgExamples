#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "../../shared/AnimationPath.h"


class TileReader : public vsg::Inherit<vsg::ReaderWriter, TileReader>
{
public:

        // defaults for readymap
        vsg::dbox extents = {{-180.0, -90.0, 0.0}, {180.0, 90.0, 1.0}};
        uint32_t noX = 2;
        uint32_t noY = 1;

        vsg::Path imageLayer;
        vsg::Path terrainLayer;
        uint32_t mipmapLevelsHint = 16;

        vsg::dbox computeTileExtents(uint32_t x, uint32_t y, uint32_t level) const;
        vsg::Path getTilePath(const vsg::Path& src, uint32_t x, uint32_t y, uint32_t level) const;

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        vsg::ref_ptr<vsg::Node> createTextureQuad(const vsg::dbox& extents, vsg::ref_ptr<vsg::Data> sourceData, uint32_t mipmapLevelsHint) const;
};

vsg::dbox TileReader::computeTileExtents(uint32_t x, uint32_t y, uint32_t level) const
{
    double multiplier = pow(0.5, double(level));
    double tileWidth = multiplier * (extents.max.x - extents.min.x) / double(noX);
    double tileHeight = multiplier * (extents.max.y - extents.min.y) / double(noY);

    vsg::dbox tile_extents;
    tile_extents.min = vsg::dvec3(double(x) * tileWidth, double(y) * tileHeight, 0.0);
    tile_extents.max = vsg::dvec3(double(x+1) * tileWidth, double(y+1) * tileHeight, 1.0);

    return tile_extents;
}

vsg::Path TileReader::getTilePath(const vsg::Path& src, uint32_t x, uint32_t y, uint32_t level) const
{
    vsg::Path path = src;

    std::stringstream sstr;

    auto replace = [&sstr](vsg::Path& path, const std::string& match, uint32_t value)
    {
        auto levelPos = path.find(match);
        sstr.clear(std::stringstream::goodbit);
        sstr.seekp(0);
        sstr << value;
        if (levelPos != std::string::npos) path.replace(levelPos, match.length(), sstr.str());
    };

    replace(path, "{z}", level);
    replace(path, "{x}", x);
    replace(path, "{y}", y);

    return path;
}

vsg::ref_ptr<vsg::Object> TileReader::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    auto extension = vsg::fileExtension(filename);
    if (extension != "tile") return {};

    std::string tile_info = filename.substr(0, filename.length()-5);
    if (tile_info == "root")
    {
        auto group = vsg::Group::create();
        uint32_t lod = 0;
        for(uint32_t y = 0; y < noY; ++y)
        {
            for(uint32_t x = 0; x < noX; ++x)
            {
                auto imagePath = getTilePath(imageLayer, x, y, lod);
                //auto terrainPath = getTilePath(terrainLayer, x, y, lod);

                auto imageTile = vsg::read_cast<vsg::Data>(imagePath, options);
                //auto terrainTile = vsg::read(terrainPath, options);

                if (imageTile)
                {
                    auto tile_extents = computeTileExtents(x, y, lod);
                    auto tile = createTextureQuad(tile_extents, imageTile, mipmapLevelsHint);

                    if (tile) group->addChild(tile);
                }
            }
        }
        return group;
    }

    std::stringstream sstr(tile_info);

    uint32_t x, y, lod;
    sstr >> x >> y >> lod;

    auto imagePath = getTilePath(imageLayer, x, y, lod);
    //auto terrainPath = getTilePath(terrainLayer, x, y, lod);

    auto imageTile = vsg::read_cast<vsg::Data>(imagePath, options);
    //auto terrainTile = vsg::read(terrainPath, options);

    //std::cout<<"   "<<imageTile<<std::endl;
    //std::cout<<"   "<<terrainTile<<std::endl;

    if (imageTile)
    {
        auto tile_extents = computeTileExtents(x, y, lod);
        auto tile = createTextureQuad(tile_extents, imageTile, mipmapLevelsHint);
        return tile;
    }

    return {};
}

vsg::ref_ptr<vsg::Node> TileReader::createTextureQuad(const vsg::dbox& extents, vsg::ref_ptr<vsg::Data> textureData, uint32_t mipmapLevelsHint) const
{
    if (!textureData) return {};

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

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
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

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto sampler = vsg::Sampler::create();
    sampler->maxLod = mipmapLevelsHint;
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    auto texture = vsg::DescriptorImage::create(sampler, textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    float min_x = extents.min.x;
    float min_y = extents.min.y;
    float max_x = extents.max.x;
    float max_y = extents.max.y;

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

    std::cout<<"tile_extents ["<<extents.min<<"], ["<<extents.max<<"] "<<scenegraph<<std::endl;

    return scenegraph;
}

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // set up vsg::Options to pass in filepaths and ReaderWriter's and other IO realted options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());

        arguments.read(options);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "vsgpagedlod";
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
        if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
        arguments.read("--screen", windowTraits->screenNum);
        arguments.read("--display", windowTraits->display);
        arguments.read("--samples", windowTraits->samples);
        auto outputFilename = arguments.value(std::string(), "-o");
        auto numFrames = arguments.value(-1, "-f");
        auto pathFilename = arguments.value(std::string(), "-p");
        auto horizonMountainHeight = arguments.value(0.0, "--hmh");
        auto mipmapLevelsHint = arguments.value<uint32_t>(0, {"--mipmapLevels", "--mml"});
        if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        auto tileReader = TileReader::create();
        tileReader->imageLayer = "http://readymap.org/readymap/tiles/1.0.0/7/{z}/{x}/{y}.jpeg";
        tileReader->terrainLayer = "http://readymap.org/readymap/tiles/1.0.0/116/{z}/{x}/{y}.tif";

        options->readerWriters.insert(options->readerWriters.begin(), tileReader);

        auto vsg_scene = vsg::read_cast<vsg::Node>("root.tile", options);
        //auto vsg_scene = vsg::read_cast<vsg::Node>("3 4 6.tile", options);

        std::cout<<"vsg_scene " <<vsg_scene<<std::endl;

        if (!vsg_scene) return 1;

        if (!outputFilename.empty())
        {
            vsg::write(vsg_scene, outputFilename);
            return 0;
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));

        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        if (vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel")); ellipsoidModel)
        {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        }

        auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

        // add close handler to respond the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        if (pathFilename.empty())
        {
            viewer->addEventHandler(vsg::Trackball::create(camera));
        }
        else
        {
            std::ifstream in(pathFilename);
            if (!in)
            {
                std::cout << "AnimationPat: Could not open animation path file \"" << pathFilename << "\".\n";
                return 1;
            }

            vsg::ref_ptr<vsg::AnimationPath> animationPath(new vsg::AnimationPath);
            animationPath->read(in);

            viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
        }

        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        viewer->compile();

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
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
