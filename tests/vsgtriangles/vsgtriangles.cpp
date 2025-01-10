#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "vsg/all.h"

std::string VERT{R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(push_constant) uniform PushConstants { mat4 projection; mat4 modelView; };
layout(location = 0) in vec3 vertex;
layout(location = 1) in vec4 color;
layout(location = 2) out vec4 rasterColor;
out gl_PerVertex { vec4 gl_Position; };
void main()
{
    rasterColor = color;
    gl_Position = (projection * modelView) * vec4(vertex, 1.0);
}
)"};

std::string FRAG{R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 2) in vec4 rasterColor;
layout(location = 0) out vec4 fragmentColor;
void main()
{
    fragmentColor = rasterColor;
}
)"};

vsg::ref_ptr<vsg::BindGraphicsPipeline> createBindGraphicsPipeline()
{
    /// Pipeline Layout: Descriptors (none in this case) and Push Constants
    vsg::DescriptorSetLayouts descriptorSetLayouts{};
    vsg::PushConstantRanges pushConstantRanges{{VK_SHADER_STAGE_VERTEX_BIT, 0, 128}};
    auto pipelineLayout = vsg::PipelineLayout::create(
        descriptorSetLayouts, pushConstantRanges);

    /// Shaders
    auto verttexShaderHints = vsg::ShaderCompileSettings::create();
    auto vertexShader = vsg::ShaderStage::create(
        VK_SHADER_STAGE_VERTEX_BIT, "main", VERT, verttexShaderHints);
    auto fragmentShaderHints = vsg::ShaderCompileSettings::create();
    auto fragmentShader = vsg::ShaderStage::create(
        VK_SHADER_STAGE_FRAGMENT_BIT, "main", FRAG, fragmentShaderHints);
    auto shaderStages = vsg::ShaderStages{vertexShader, fragmentShader};

    /// Vertex Attributes
    auto vertexInputState = vsg::VertexInputState::create();
    auto& bindings = vertexInputState->vertexBindingDescriptions;
    auto& attributes = vertexInputState->vertexAttributeDescriptions;

    uint32_t bindingIndex{0};
    uint32_t location{0};
    constexpr uint32_t offset{0};
    bindings.emplace_back(VkVertexInputBindingDescription{
        bindingIndex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX});
    attributes.emplace_back(VkVertexInputAttributeDescription{
        bindingIndex, location, VK_FORMAT_R32G32B32_SFLOAT, offset});

    bindingIndex = 1;
    location = 1;
    bindings.emplace_back(VkVertexInputBindingDescription{
        bindingIndex, sizeof(vsg::vec4), VK_VERTEX_INPUT_RATE_VERTEX});
    attributes.emplace_back(VkVertexInputAttributeDescription{
        bindingIndex, location, VK_FORMAT_R32G32B32A32_SFLOAT, offset});

    /// Pipeline States
    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    auto depthStencilState = vsg::DepthStencilState::create();
    depthStencilState->depthTestEnable = VK_TRUE;
    auto graphicsPipelineStates = vsg::GraphicsPipelineStates{
        vertexInputState,
        inputAssemblyState,
        rasterizationState,
        vsg::ColorBlendState::create(),
        vsg::MultisampleState::create(),
        depthStencilState};

    /// Graphics Pipeline
    auto graphicsPipeline = vsg::GraphicsPipeline::create(
        pipelineLayout, shaderStages, graphicsPipelineStates);
    return vsg::BindGraphicsPipeline::create(graphicsPipeline);
}

vsg::ref_ptr<vsg::VertexIndexDraw> createDrawCommands(
    vsg::uivec3 numTriangles,
    vsg::vec3 posMin,
    vsg::vec3 posRng)
{
    size_t const numVertices = 3 * numTriangles.x * numTriangles.y * numTriangles.z;

    float const dx{posRng.x / numTriangles.x};
    float const dy{posRng.y / numTriangles.y};
    float const dz{0.5f * posRng.z / numTriangles.z};

    auto vertices = vsg::vec3Array::create(numVertices);
    auto colors = vsg::vec4Array::create(numVertices);
    auto indices = vsg::uintArray::create(numVertices);

    size_t i{0};
    float cx, cy, cz;
    float x, y, z;
    for (size_t xi{0}; xi < numTriangles.x; ++xi)
    {
        cx = static_cast<float>(xi) / static_cast<float>(numTriangles.x);
        x = posMin.x + cx * posRng.x;
        for (size_t yi{0}; yi < numTriangles.y; ++yi)
        {
            cy = static_cast<float>(yi) / static_cast<float>(numTriangles.y);
            y = posMin.y + cy * posRng.y;
            for (size_t zi{0}; zi < numTriangles.z; ++zi)
            {
                cz = static_cast<float>(zi) / static_cast<float>(numTriangles.z);
                z = posMin.z + cz * posRng.z;

                assert((i + 2) < numVertices);

                vertices->at(i) = vsg::vec3{x, y, z};
                colors->at(i) = vsg::vec4{cx, cy, cz, 1.0f};
                indices->at(i) = i;
                ++i;

                vertices->at(i) = vsg::vec3{x + dx, y, z};
                colors->at(i) = vsg::vec4{cz, cx, cy, 1.0f};
                indices->at(i) = i;
                ++i;

                vertices->at(i) = vsg::vec3{x, y + dy, z + dz};
                colors->at(i) = vsg::vec4{cy, cz, cx, 1.0f};
                indices->at(i) = i;
                ++i;
            }
        }
    }

    vsg::DataList vertexArrays{vertices, colors};
    auto drawCommands = vsg::VertexIndexDraw::create();
    drawCommands->assignArrays(vertexArrays);
    drawCommands->assignIndices(indices);
    drawCommands->indexCount = indices->width();
    drawCommands->instanceCount = 1;
    return drawCommands;
}

vsg::ref_ptr<vsg::Group> createScene(
    size_t numPipelines,
    size_t numDrawCalls,
    vsg::uivec3 numTrianglesVec)
{
    auto sceneGraph = vsg::Group::create();

    size_t totalTriangles{0};
    size_t totalVertices{0};
    size_t totalBytes{0};

    for (size_t pi{0}; pi < numPipelines; ++pi)
    {

        /// Graphics Pipeline State Group
        auto stateGroup = vsg::StateGroup::create();
        auto bindGraphicsPipeline = createBindGraphicsPipeline();
        stateGroup->add(bindGraphicsPipeline);

        /// Draw Commands
        ///     pipelines are slabs over y
        ///     individual draw calls are slabs over z
        for (size_t di{0}; di < numDrawCalls; ++di)
        {
            vsg::uivec3 numTri{
                numTrianglesVec.x,
                static_cast<uint32_t>(std::ceil(numTrianglesVec.y / numPipelines)),
                static_cast<uint32_t>(std::ceil(numTrianglesVec.z / numDrawCalls))};
            vsg::vec3 posMin{
                -100.0f,
                -100.0f + static_cast<float>(pi) * 200.0f / numPipelines,
                -100.0f + static_cast<float>(di) * 200.0f / numDrawCalls};
            vsg::vec3 posRng{
                200.0f,
                200.0f / numPipelines,
                200.0f / numDrawCalls};
            auto drawCommands = createDrawCommands(numTri, posMin, posRng);
            stateGroup->addChild(drawCommands);

            size_t numTriangles = numTri.x * numTri.y * numTri.z;
            size_t numVertices = 3 * numTriangles;
            totalTriangles += numTriangles;
            totalVertices += numVertices;
            totalBytes += numVertices * (sizeof(vsg::vec3) + sizeof(vsg::vec4) + sizeof(uint32_t));
        }

        sceneGraph->addChild(stateGroup);
    }

    std::cout << "number of triangles: " << totalTriangles << std::endl;
    std::cout << "number of vertices:  " << totalVertices << std::endl;
    std::cout << "size of scene: " << totalBytes / std::pow(2, 20) << " MB\n";

    return sceneGraph;
}

int main(int argc, char** argv)
{
    try
    {
        vsg::CommandLine arguments(&argc, argv);

        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);

        auto numPipelines = arguments.value(1u, {"--numPipelines", "-p"});
        auto numDrawCalls = arguments.value(1u, {"--numDrawCalls", "-c"});
        auto numTriangles = arguments.value(vsg::uivec3{10, 10, 10}, {"--numTriangles", "-n"});

        auto sceneGraph = createScene(numPipelines, numDrawCalls, numTriangles);

        /// Window
        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        windowTraits->windowTitle = "triangles";
        windowTraits->samples = VK_SAMPLE_COUNT_4_BIT;
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Unable to create window" << std::endl;
            return 1;
        }
        auto gpuNumber = arguments.value(-1, "--gpu");
        if (gpuNumber == -1)
        {
            (void)window->getOrCreatePhysicalDevice();
        }
        else
        {
            auto instance = window->getOrCreateInstance();
            (void)window->getOrCreateSurface(); // fulfill contract of getOrCreatePhysicalDevice();
            auto& physicalDevices = instance->getPhysicalDevices();
            window->setPhysicalDevice(physicalDevices[gpuNumber]);
        }

        /// Camera
        auto lookAt = vsg::LookAt::create(
            vsg::dvec3{150, 175, 200},
            vsg::dvec3{0, 0, 0},
            vsg::dvec3{0, 0, 1});
        auto perspective = vsg::Perspective::create();
        auto viewportState = vsg::ViewportState::create(window->extent2D());
        auto camera = vsg::Camera::create(perspective, lookAt, viewportState);

        /// Viewer and CommandGraph
        auto viewer = vsg::Viewer::create();
        viewer->addWindow(window);
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto commandGraph = vsg::CommandGraph::create(window);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        /// View and RenderGraph
        auto view = vsg::View::create(camera);
        auto renderGraph = vsg::RenderGraph::create(window, view);
        commandGraph->addChild(renderGraph);

        /// Add the scene to the View and compile
        view->addChild(sceneGraph);
        viewer->compile();

        while (viewer->advanceToNextFrame())
        {
            viewer->handleEvents();
            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();
        }
    }
    catch (const vsg::Exception& ve)
    {
        // details of VkResult values:
        // https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#VkResult
        for (int i = 0; i < argc; ++i) { std::cerr << argv[i] << " "; }
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }
}
