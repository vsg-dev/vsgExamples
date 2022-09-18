#include "TextGroup.h"

#include <vsg/all.h>
#include <iostream>

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

    // set up text group
    auto textgroup = vsg::TextGroup::create();

    {
        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->position = vsg::vec3(6.0, 0.0, 0.0);
            layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
            layout->outlineWidth = 0.1;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("VulkanSceneGraph now\nhas SDF text support.");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->glyphLayout = vsg::StandardLayout::VERTICAL_LAYOUT;
            layout->position = vsg::vec3(-1.0, 0.0, 2.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 0.0, 0.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("VERTICAL_LAYOUT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->glyphLayout = vsg::StandardLayout::LEFT_TO_RIGHT_LAYOUT;
            layout->position = vsg::vec3(-1.0, 0.0, 2.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(0.0, 1.0, 0.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("LEFT_TO_RIGHT_LAYOUT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->glyphLayout = vsg::StandardLayout::RIGHT_TO_LEFT_LAYOUT;
            layout->position = vsg::vec3(13.0, 0.0, 2.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(0.0, 0.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("RIGHT_TO_LEFT_LAYOUT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->position = vsg::vec3(2.0, 0.0, -8.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 0.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("horizontalAlignment\nCENTER_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::LEFT_ALIGNMENT;
            layout->position = vsg::vec3(2.0, 0.0, -9.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 1.0, 0.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("horizontalAlignment\nLEFT_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::RIGHT_ALIGNMENT;
            layout->position = vsg::vec3(2.0, 0.0, -10.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(0.0, 1.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("horizontalAlignment\nRIGHT_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->verticalAlignment = vsg::StandardLayout::BOTTOM_ALIGNMENT;
            layout->position = vsg::vec3(10.0, 0.0, -8.5);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(0.0, 1.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("verticalAlignment\nBOTTOM_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->verticalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->position = vsg::vec3(10.0, 0.0, -9.0);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 0.0, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("verticalAlignment\nCENTER_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }

        {
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
            layout->verticalAlignment = vsg::StandardLayout::TOP_ALIGNMENT;
            layout->position = vsg::vec3(10.0, 0.0, -9.5);
            layout->horizontal = vsg::vec3(0.5, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 0.5);
            layout->color = vsg::vec4(1.0, 1.0, 0.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("verticalAlignment\nTOP_ALIGNMENT");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }
        if (output_filename.empty())
        {
            struct CustomLayout : public vsg::Inherit<vsg::StandardLayout, CustomLayout>
            {
                void layout(const vsg::Data* text, const vsg::Font& font, vsg::TextQuads& quads) override
                {
                    // Let the base StandardLayout class do the basic glyph setup
                    Inherit::layout(text, font, quads);

                    // modify each generated glyph quad's position and colours etc.
                    for (auto& quad : quads)
                    {
                        for (int i = 0; i < 4; ++i)
                        {
                            quad.vertices[i].z += 0.5f * sin(quad.vertices[i].x);
                            quad.colors[i].r = 0.5f + 0.5f * sin(quad.vertices[i].x);
                            quad.outlineColors[i] = vsg::vec4(cos(0.5 * quad.vertices[i].x), 0.1f, 0.0f, 1.0f);
                            quad.outlineWidths[i] = 0.1f + 0.15f * (1.0f + sin(quad.vertices[i].x));
                        }
                    }
                };
            };

            auto layout = CustomLayout::create();
            layout->position = vsg::vec3(0.0, 0.0, -3.0);
            layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
            layout->color = vsg::vec4(1.0, 0.5, 1.0, 1.0);

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("You can use Outlines\nand your own CustomLayout.");
            text->font = font;
            text->layout = layout;
            textgroup->addChild(text);
        }
    }

    if (!output_filename.empty())
    {
        vsg::write(textgroup, output_filename);
        return 1;
    }

    textgroup->setup();

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
