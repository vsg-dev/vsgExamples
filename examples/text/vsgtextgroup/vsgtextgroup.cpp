#include <iostream>
#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create("vsg::TextGroup example");
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    auto font_filename = arguments.value(std::string("fonts/times.vsgb"), "-f");
    auto output_filename = arguments.value(std::string(), "-o");
    auto numFrames = arguments.value(-1, "--nf");
    auto clearColor = arguments.value(vsg::vec4(0.2f, 0.2f, 0.4f, 1.0f), "--clear");
    if (arguments.read("-t"))
    {
        windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        windowTraits->width = 192, windowTraits->height = 108;
        windowTraits->decoration = false;
    }
    uint32_t numLabels = arguments.value(1000, "-n");
    bool billboard = arguments.read({"-b", "--billboard"});
    bool disableDepthTest = arguments.read({"--ddt", "--disable-depth-test"});
    float billboardAutoScaleDistance = arguments.value(100.0f, "--distance");

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
        std::cout << "Failed to read font : " << font_filename << std::endl;
        return 1;
    }

    if (disableDepthTest)
    {
        // assign a custom ShaderSet to options->shaderSets so that subsequent TextGroup::setup(0, options) call will pass in our custom ShaderSet.
        auto shaderSet = options->shaderSets["text"] = vsg::createTextShaderSet(options);

        // create a DepthStencilState, disable depth test and add this to the ShaderSet::defaultGraphicsPipelineStates container so it's used when setting up the TextGroup subgraph
        auto depthStencilState = vsg::DepthStencilState::create();
        depthStencilState->depthTestEnable = VK_FALSE;
        shaderSet->defaultGraphicsPipelineStates.push_back(depthStencilState);
    }

    // set up text group
    auto textgroup = vsg::TextGroup::create();

    double numBlocks = ceil(static_cast<double>(numLabels) / 10.0);
    uint32_t numColumns = static_cast<uint32_t>(ceil(sqrt(numBlocks)));
    uint32_t numRows = static_cast<uint32_t>(ceil(static_cast<double>(numBlocks) / static_cast<double>(numColumns)));
    float size = 1.0f;

    vsg::vec3 row_origin(0.0f, 0.0f, 0.0f);
    vsg::vec3 dx(20.0f, 0.0f, 0.0f);
    vsg::vec3 dy(0.0f, 20.0f, 0.0f);
    vsg::vec3 horizontal = vsg::vec3(size, 0.0, 0.0);
    vsg::vec3 vertical = billboard ? vsg::vec3(0.0, size, 0.0) : vsg::vec3(0.0, 0.0, size);

    for (uint32_t r = 0; r < numRows; ++r)
    {
        vsg::vec3 local_origin = row_origin;
        for (uint32_t c = 0; c < numColumns; ++c)
        {

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                //layout->verticalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                layout->position = local_origin + vsg::vec3(6.0, 0.0, 0.0);
                layout->horizontal = horizontal;
                layout->vertical = vertical;
                layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
                layout->outlineWidth = 0.1f;
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("VulkanSceneGraph now\nhas SDF text support.");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->glyphLayout = vsg::StandardLayout::VERTICAL_LAYOUT;
                layout->position = local_origin + vsg::vec3(-1.0, 0.0, 2.0);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(1.0, 0.0, 0.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("VERTICAL_LAYOUT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->glyphLayout = vsg::StandardLayout::LEFT_TO_RIGHT_LAYOUT;
                layout->position = local_origin + vsg::vec3(-1.0, 0.0, 2.0);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(0.0, 1.0, 0.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("LEFT_TO_RIGHT_LAYOUT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->glyphLayout = vsg::StandardLayout::RIGHT_TO_LEFT_LAYOUT;
                layout->position = local_origin + vsg::vec3(13.0, 0.0, 2.0);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(0.0, 0.0, 1.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("RIGHT_TO_LEFT_LAYOUT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                layout->position = local_origin + vsg::vec3(2.0, 0.0, -8.0);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(1.0, 0.0, 1.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("horizontalAlignment\nCENTER_ALIGNMENT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->horizontalAlignment = vsg::StandardLayout::LEFT_ALIGNMENT;
                layout->position = local_origin + vsg::vec3(2.0, 0.0, -9.0);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(1.0, 1.0, 0.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("horizontalAlignment\nLEFT_ALIGNMENT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->horizontalAlignment = vsg::StandardLayout::RIGHT_ALIGNMENT;
                layout->position = local_origin + vsg::vec3(2.0, 0.0, -10.0);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(0.0, 1.0, 1.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("horizontalAlignment\nRIGHT_ALIGNMENT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                layout->verticalAlignment = vsg::StandardLayout::BOTTOM_ALIGNMENT;
                layout->position = local_origin + vsg::vec3(10.0, 0.0, -8.5);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(0.0, 1.0, 1.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("verticalAlignment\nBOTTOM_ALIGNMENT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                layout->verticalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                layout->position = local_origin + vsg::vec3(10.0, 0.0, -9.0);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(1.0, 0.0, 1.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("verticalAlignment\nCENTER_ALIGNMENT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels)
            {
                auto layout = vsg::StandardLayout::create();
                layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                layout->verticalAlignment = vsg::StandardLayout::TOP_ALIGNMENT;
                layout->position = local_origin + vsg::vec3(10.0, 0.0, -9.5);
                layout->horizontal = horizontal * 0.5f;
                layout->vertical = vertical * 0.5f;
                layout->color = vsg::vec4(1.0, 1.0, 0.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("verticalAlignment\nTOP_ALIGNMENT");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            if (textgroup->children.size() < numLabels && output_filename.empty())
            {
                struct CustomLayout : public vsg::Inherit<vsg::StandardLayout, CustomLayout>
                {
                    void layout(const vsg::Data* text, const vsg::Font& font, vsg::TextQuads& quads) override
                    {
                        // Let the base StandardLayout class do the basic glyph setup
                        size_t start_of_text = quads.size();

                        Inherit::layout(text, font, quads);

                        // modify each generated glyph quad's position and colours etc.
                        for (size_t qi = start_of_text; qi < quads.size(); ++qi)
                        {
                            auto& quad = quads[qi];
                            for (int i = 0; i < 4; ++i)
                            {
                                quad.vertices[i].z += 0.5f * sin(quad.vertices[i].x);
                                quad.colors[i].r = 0.5f + 0.5f * sin(quad.vertices[i].x);
                                quad.outlineColors[i] = vsg::vec4(cos(0.5f * quad.vertices[i].x), 0.1f, 0.0f, 1.0f);
                                quad.outlineWidths[i] = 0.1f + 0.15f * (1.0f + sin(quad.vertices[i].x));
                            }
                        }
                    };
                };

                auto layout = CustomLayout::create();
                layout->position = local_origin + vsg::vec3(0.0, 0.0, -3.0);
                layout->horizontal = horizontal;
                layout->vertical = vertical;
                layout->color = vsg::vec4(1.0, 0.5, 1.0, 1.0);
                layout->billboard = billboard;
                layout->billboardAutoScaleDistance = billboardAutoScaleDistance;

                auto text = vsg::Text::create();
                text->text = vsg::stringValue::create("You can use Outlines\nand your own CustomLayout.");
                text->font = font;
                text->layout = layout;
                textgroup->addChild(text);
            }

            local_origin += dx;
        }
        row_origin += dy;
    }

    textgroup->setup(0, options);

    if (!output_filename.empty())
    {
        vsg::write(textgroup, output_filename);
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

    window->clearColor() = vsg::sRGB_to_linear(clearColor);

    viewer->addWindow(window);

    // camera related details
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    textgroup->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 1000.0);
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    auto commandGraph = vsg::createCommandGraphForView(window, camera, textgroup);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile the Vulkan objects
    viewer->compile();

    // assign Trackball
    viewer->addEventHandler(vsg::Trackball::create(camera));

    // assign a CloseHandler to the Viewer to respond to pressing Escape or the window close button
    viewer->addEventHandlers({vsg::CloseHandler::create(viewer)});

    auto startTime = vsg::clock::now();
    double numFramesCompleted = 0.0;

    // main frame loop
    while (viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        viewer->update();

        viewer->recordAndSubmit();

        viewer->present();

        numFramesCompleted += 1.0;
    }

    auto duration = std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - startTime).count();
    if (numFramesCompleted > 0.0)
    {
        std::cout << "Average frame rate = " << (numFramesCompleted / duration) << std::endl;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
