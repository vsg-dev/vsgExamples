//======================================================================
// This programs allows playing with the fragment shader. The syntax
// of the shader programs is based on the shadertoy syntax at
// https://www.shadertoy.com/new. Note that currently only
// the following uniforms are supported:
//
// uniform vec2 iResolution; // viewport resolution (in pixels)
// uniform float iTime;      // shader playback time (in seconds)
// uniform vec2 iMouse;      // mouse pixel coords. xy: current 
// uniform int iFrame;       // shader playback frame
//
// However it is enough to run a large number of shadertoy shaders.
//
// 2024-07-20 Sat
// Dov Grobgeld <dov.grobgeld@gmail.com>
//----------------------------------------------------------------------

#include <iostream>
#include <vsg/all.h>
#include <chrono>

using namespace std;

struct ToyUniform {
    vsg::ivec2 iResolution;
    vsg::vec2 iMouse;
    float iTime;
    int iFrame;
};
using ToyUniformValue = vsg::Value<ToyUniform>;

// The fixed toy vertex shader
const auto shadertoy_vert = R"(
#version 450

layout(set = 0, binding = 0) uniform UBO {
    ivec2 iResolution;
    vec2 iMouse;
    float iTime;
    int iFrame;
} ubo;

layout(location = 0) in vec2 inVertex;
layout(location = 1) in vec2 inTexture;

layout(location = 0) out vec2 fragCoord;

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    vec4 vertex = vec4(inVertex, 0, 1.0);

    gl_Position = vec4(inVertex.x, -inVertex.y, 0.5, 1.0);

    fragCoord = vec2(inTexture.x * ubo.iResolution.x,
                     (1-inTexture.y) * ubo.iResolution.y);
}
)";


string readFile(const string& filename)
{
    ifstream fh(filename);

    if (!fh.good())
        throw runtime_error(std::string("Error opening file \"") + filename + "\"  for input!");

    string ret;
    ret.assign((istreambuf_iterator<char>(fh)), istreambuf_iterator<char>());
    fh.close();
    return ret;
}

const string defaultShader = R"(
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized coordinates
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
}
)";

// The default shader is composed of three dancing spheres
string shaderToyToFragmentShader(const string& toyShader)
{
  return R"(
#version 450

layout(set = 0, binding = 0) uniform UBO {
    ivec2 iResolution;
    vec2 iMouse;
    float iTime;
    int iFrame;
} ubo;

layout(location = 0) in vec2 fragCoord;
layout(location = 0) out vec4 fragColor;

// Create shortcuts to the uniform buffer. This could a
// be solved by a search and replace of the shader string.
ivec2 iResolution = ubo.iResolution;
float iTime = ubo.iTime;

vec2 iMouse = ubo.iMouse;
int iFrame = ubo.iFrame;

// This should be exactly shadertoy syntax
)" + toyShader + R"(
void main()
{
   mainImage(fragColor, fragCoord);
})";

}


// Create a vsg node containing an image
vsg::ref_ptr<vsg::Node> createToyNode(string toyShader,
                                      // output
                                      vsg::ref_ptr<ToyUniformValue>& toyUniform)
{
    // load shaders
    auto vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", shadertoy_vert);
    
    auto fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", shaderToyToFragmentShader(toyShader));

    if (!vertexShader || !fragmentShader)
        throw std::runtime_error("Could not create shaders.");

    toyUniform = ToyUniformValue::create();
    toyUniform->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    toyUniform->value().iResolution = {800,600};
    toyUniform->value().iTime = 0;
    toyUniform->value().iFrame = 0;
    toyUniform->value().iMouse = vsg::vec2 {0,0};
    toyUniform->dirty();

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32_SFLOAT, 0}     // tex coord data
    };

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()};

    // Do I need this when having a constant V,M, and P?
    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
    };

    auto toyUniformDescriptor = vsg::DescriptorBuffer::create(toyUniform, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{toyUniformDescriptor});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto node = vsg::StateGroup::create();
    node->add(bindGraphicsPipeline);
    node->add(bindDescriptorSet);

    // set up vertex and index arrays
    auto vertices = vsg::vec2Array::create(
        {{-1.0f, -1.0f},
         {1.0f, -1.0f},
         {1.0f, 1.0f},
         {-1.0f, 1.0f}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 1.0f},
         {1.0f, 1.0f},
         {1.0f, 0.0f},
         {0.0f, 0.0f}}); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0}); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    // add drawCommands to transform
    node->addChild(drawCommands);

    return node;
}

class MouseHandler : public vsg::Inherit<vsg::Visitor, MouseHandler>
{
public:
    void apply(vsg::PointerEvent& pointerEvent) override
    {
        lastPointerEvent = &pointerEvent;
    }
    

    vsg::ref_ptr<vsg::PointerEvent> lastPointerEvent;
};

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);


    windowTraits->height = 600;
    windowTraits->width = 800;

    auto scene = vsg::Group::create();

    // Read the shader from the command line
    std::string toyShader;
    if (argc < 2)
    {
        windowTraits->windowTitle = "vsgshadertoy";
        toyShader = defaultShader;
    }
    else {
        toyShader = readFile(argv[1]);
        string title;
        // extract the filename from the path
        auto pos = string(argv[1]).find_last_of("/");
        if (pos != string::npos)
            title = string(argv[1]).substr(pos+1);
        else
            title = argv[1];
        
        windowTraits->windowTitle = title;
    }

    
    // Add the image to the scene
    vsg::ref_ptr<ToyUniformValue> toyUniform;
    scene->addChild(createToyNode(toyShader,
                                  // output
                                  toyUniform
                                  ));
    toyUniform->dirty();

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // camera related details
    auto viewport = vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height);
    auto ortho = vsg::Orthographic::create();
    ortho->nearDistance = -1000;
    ortho->farDistance = 1000;
    auto lookAt = vsg::LookAt::create(vsg::dvec3(0, 0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
    auto camera = vsg::Camera::create(ortho, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    // assign a CloseHandler to the Viewer to respond to pressing Escape or the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // Handle the mouse and resize
    auto mouseHandler = MouseHandler::create();
    viewer->addEventHandler(mouseHandler);
    
    // main frame loop
    auto t0 = std::chrono::steady_clock::now();
    while (viewer->advanceToNextFrame())
    {
        auto extent = window->extent2D();
        toyUniform->value().iResolution = {(int)extent.width, (int)extent.height};
        toyUniform->value().iTime = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
        toyUniform->value().iFrame += 1;
        toyUniform->value().iMouse = mouseHandler->lastPointerEvent ? vsg::vec2(mouseHandler->lastPointerEvent->x, extent.height-mouseHandler->lastPointerEvent->y) : vsg::vec2(0,0);

        toyUniform->dirty();

        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
