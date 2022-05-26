#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

vsg::ref_ptr<vsg::ShaderSet> createMyTextShaderSet(const vsg::Path& vertesShaderFilename,  const vsg::Path& fragmentShaderFilename, vsg::ref_ptr<const vsg::Options> options)
{
    // based on VulkanSceneGraph/src/text/Text.cpp's vsg::createTextShaderSet(ref_ptr<const Options> options) implementation

    // load custom vertex shaders
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>(vertesShaderFilename, options);
    // if (!vertexShader) vertexShader = text_vert(); // fallback to shaders/text_vert.cppp

    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>(fragmentShaderFilename, options);
    // if (!fragmentShader) fragmentShader = text_frag(); // fallback to shaders/text_frag.cppp

    uint32_t numTextIndices = 256;
    vertexShader->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::uintValue::create(numTextIndices)} // numTextIndices
    };

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    // used for both CPU and GPU layouts
    shaderSet->addAttributeBinding("inPosition", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addUniformBinding("textureAtlas", "", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    // only used when using CPU Layout
    shaderSet->addAttributeBinding("inColor", "CPU_LAYOUT", 1, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
    shaderSet->addAttributeBinding("inOutlineColor", "CPU_LAYOUT", 2, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
    shaderSet->addAttributeBinding("inOutlineWidth", "CPU_LAYOUT", 3, VK_FORMAT_R32_SFLOAT, vsg::floatArray::create(1));
    shaderSet->addAttributeBinding("inTexCoord", "CPU_LAYOUT", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    // only used when using GPU Layout
    shaderSet->addUniformBinding("glyphMetrics", "GPU_LAYOUT", 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addUniformBinding("textLayout", "GPU_LAYOUT", 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::TextLayoutValue::create());
    shaderSet->addUniformBinding("text", "GPU_LAYOUT", 1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::uivec4Array2D::create(1, 1));

    return shaderSet;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create("vsg::Text example");
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    auto font_filename = arguments.value(std::string("fonts/times.vsgb"), "-f");
    auto output_filename = arguments.value(std::string(), "-o");
    auto numFrames = arguments.value(-1, "--nf");
    auto clearColor = arguments.value(vsg::vec4(0.2f, 0.2f, 0.4f, 1.0f), "--clear");

    auto vertesShaderFilename = arguments.value<vsg::Path>("shaders/custom_text.vert", "--vert");
    auto fragmentShaderFilename = arguments.value<vsg::Path>("shaders/custom_text.frag", "--frag");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    auto font = vsg::read_cast<vsg::Font>(font_filename, options);

    if (!font)
    {
        std::cout << "Failing to read font : " << font_filename << std::endl;
        return 1;
    }

    font->options = options;

    // set up model transformation node
    auto scenegraph = vsg::Group::create();

    {
        auto layout = vsg::StandardLayout::create();
        layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        layout->position = vsg::vec3(0.0, 0.0, 0.0);
        layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
        layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
        layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
        layout->outlineWidth = 0.1;

        auto text = vsg::Text::create();
        text->text = vsg::stringValue::create("Standard vsg::Text shaders.");
        text->font = font;
        text->layout = layout;
        text->setup();
        scenegraph->addChild(text);
    }

    {
        font->options->shaderSets["text"] = createMyTextShaderSet(vertesShaderFilename, fragmentShaderFilename, options);

        auto layout = vsg::StandardLayout::create();
        layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        layout->position = vsg::vec3(0.0, -2.0, 0.0);
        layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
        layout->vertical = vsg::vec3(0.0, 1.0, 0.0);
        layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
        layout->outlineWidth = 0.1;

        auto text = vsg::Text::create();
        text->text = vsg::stringValue::create("Custom vsg::Text shaders.");
        text->font = font;
        text->layout = layout;
        text->setup();
        scenegraph->addChild(text);
    }

    if (!output_filename.empty())
    {
        vsg::write(scenegraph, output_filename);
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    window->clearColor() = VkClearColorValue{{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};

    viewer->addWindow(window);

    // camera related details
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 1000.0);
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    // assign Trackball
    viewer->addEventHandler(vsg::Trackball::create(camera));

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    // main frame loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
