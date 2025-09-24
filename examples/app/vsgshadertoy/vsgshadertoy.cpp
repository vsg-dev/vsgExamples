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

layout(location = 0) out vec2 fragCoord;

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    // fragCord is from (0→w,0→h)
    fragCoord = vec2((gl_VertexIndex << 1) & 2,
                     (gl_VertexIndex & 2)) * ubo.iResolution;

    // gl_Position is from (-1→1,-1→1)
    gl_Position = vec4(fragCoord.x/ubo.iResolution.x * 2.0 - 1.0,
                       (1.0-fragCoord.y/ubo.iResolution.y) * 2.0 - 1.0,
                       0.5, 1.0);
}
)";


std::string readFile(const vsg::Path& filename)
{
    std::ifstream fh(filename);

    if (!fh.good())
        throw std::runtime_error(std::string("Error opening file \"") + filename + "\"  for input!");

    std::string ret;
    ret.assign((std::istreambuf_iterator<char>(fh)),
               std::istreambuf_iterator<char>());
    fh.close();
    return ret;
}

const std::string defaultShader = R"(
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

std::string shaderToyToFragmentShader(const std::string& toyShader)
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


// Create a vsg node containing the shadertoy command
vsg::ref_ptr<vsg::Node> createToyNode(const std::string& toyShader,
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

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(), // No vertices for shader toy
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()};

    auto toyUniformDescriptor = vsg::DescriptorBuffer::create(toyUniform, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges {});
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{toyUniformDescriptor});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto node = vsg::StateGroup::create();
    node->add(bindGraphicsPipeline);
    node->add(bindDescriptorSet);

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::Draw::create(3, 1, 0, 0)); // Draw without vertices, as they are generated from gl_VertexIndex

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
        isPressed = pointerEvent.mask != vsg::BUTTON_MASK_OFF;
    }
    
    vsg::ref_ptr<vsg::PointerEvent> lastPointerEvent;
    bool isPressed = false;
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
    else
    {
        vsg::Path filePath(argv[1]);
        toyShader = readFile(filePath);

        windowTraits->windowTitle = std::string("vsgshadertoy: ") + vsg::simpleFilename(filePath) + vsg::fileExtension(filePath);
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

        if (mouseHandler->isPressed)
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
