#include <vsg/all.h>
#include <iostream>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#endif


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    auto font_filename = arguments.value(std::string("fonts/times.vsgb"), "-f");
    auto output_filename = arguments.value(std::string(), "-o");
    auto render_all_glyphs = arguments.read("--all");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto options = vsg::Options::create();
    options->paths = searchPaths;
    #ifdef USE_VSGXCHANGE
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
    #endif

    auto font = vsg::read_cast<vsg::Font>(font_filename, options);

    if (!font)
    {
        std::cout<<"Failling to read font : "<<font_filename<<std::endl;
        return 1;
    }

    // set up model transformation node
    auto scenegraph = vsg::Group::create();

    if (render_all_glyphs)
    {
#if 0
        struct CustomLayout : public vsg::Inherit<vsg::LeftAlignment, CustomLayout>
        {
            void layout(const vsg::Data* text, const vsg::Font& font, vsg::TextQuads& quads) override
            {
                Inherit::layout(text, font, quads);

                vsg::vec4 color0(1.0f, 1.0f, 1.0f, 1.0f);
                vsg::vec4 color1(1.0f, 0.0f, 1.0f, 1.0f);

                int qi = 0;
                for(auto& quad : quads)
                {
                    for(int i=0; i<4; ++i)
                    {
                        quad.colors[i] = (qi%2) ? color1 : color0;
                    }

                    for(int i=1; i<4; ++i)
                    {
                        quad.vertices[i] = quad.vertices[0] + (quad.vertices[i]-quad.vertices[0])*0.5f;
                    }

                    ++qi;
                }
            };
        };

        auto layout = CustomLayout::create();
#else
        auto layout = vsg::LeftAlignment::create();
#endif
        layout->position = vsg::vec3(0.0, 0.0, 0.0);
        layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
        layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
        layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);

        size_t row_lenghth = static_cast<size_t>(ceil(sqrt(float(font->glyphs.size()))));
        size_t num_rows = font->glyphs.size() / row_lenghth;
        if ((font->glyphs.size() % num_rows) != 0) ++num_rows;

        auto text_string = vsg::uintArray::create(font->glyphs.size() + num_rows - 1);
        auto text_itr = text_string->begin();

        size_t i = 0;
        size_t column = 0;

        for(auto& glyph : font->glyphs)
        {
            ++i;
            ++column;

            (*text_itr++) = glyph.second.charcode;

            if (column >= row_lenghth)
            {
                (*text_itr++) = '\n';
                column = 0;
            }
        }

        auto text = vsg::Text::create();
        text->font = font;
        text->layout = layout;
        text->text = text_string;
        text->setup();

        scenegraph->addChild(text);
    }
    else
    {
        {
            auto layout = vsg::LeftAlignment::create();
            layout->position = vsg::vec3(0.0, 0.0, 0.0);
            layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
            layout->vertical = vsg::vec3(0.0, 0.0, 1.0);
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
            layout->outlineWidth = 0.2;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create("VulkanSceneGraph now\nhas SDF text support.");
            text->font = font;
            text->layout = layout;
            text->setup();
            scenegraph->addChild(text);
        }

        {
            struct CustomLayout : public vsg::Inherit<vsg::LeftAlignment, CustomLayout>
            {
                void layout(const vsg::Data* text, const vsg::Font& font, vsg::TextQuads& quads) override
                {
                    // Let the base LeftAlignment class do the basic glyph setup
                    Inherit::layout(text, font, quads);

                    // modify each generated glyph quad's position and colours etc.
                    for(auto& quad : quads)
                    {
                        for(int i=0; i<4; ++i)
                        {
                            quad.vertices[i].z += 0.5f * sin(quad.vertices[i].x);
                            quad.colors[i].r = 0.5f + 0.5f*sin(quad.vertices[i].x);
                            quad.outlineColors[i] = vsg::vec4(cos(0.5*quad.vertices[i].x), 0.1f, 0.0f, 1.0f);
                            quad.outlineWidths[i] = 0.1f + 0.15f*(1.0f+sin(quad.vertices[i].x));
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
            text->setup();
            scenegraph->addChild(text);
        }
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
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // camera related details
    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    scenegraph->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.001;

    // set up the camera
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 1000.0);
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
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
    while (viewer->advanceToNextFrame())
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
