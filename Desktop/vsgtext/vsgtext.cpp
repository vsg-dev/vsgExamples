#include <vsg/all.h>
#include <iostream>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#endif

namespace text
{

    vsg::ref_ptr<vsg::Font> readUnity3dFontMetaFile(const std::string& font_metricsFile, vsg::ref_ptr<vsg::Options> options)
    {
        auto metricsFilePath = vsg::findFile(font_metricsFile, options);

        if (metricsFilePath.empty()) return {};

        auto font = vsg::Font::create();
        font->options = options;

        // read glyph data from txt file
        auto startsWith = [](const std::string& str, const std::string& match)
        {
            return str.compare(0, match.length(), match) == 0;
        };

        auto split = [](const std::string& str, const char& seperator)
        {
            std::vector<std::string> elements;

            std::string::size_type prev_pos = 0, pos = 0;

            while ((pos = str.find(seperator, pos)) != std::string::npos)
            {
                auto substring = str.substr(prev_pos, pos - prev_pos);
                elements.push_back(substring);
                prev_pos = ++pos;
            }

            elements.push_back(str.substr(prev_pos, pos - prev_pos));

            return elements;
        };

        auto uintValueFromPair = [&split](const std::string& str)
        {
            std::vector<std::string> els = split(str, '=');
            return static_cast<uint32_t>(atoi(els[1].c_str()));
        };

        auto floatValueFromPair = [&split](const std::string& str)
        {
            std::vector<std::string> els = split(str, '=');
            return static_cast<float>(atof(els[1].c_str()));
        };

        auto stringValueFromPair = [&split](const std::string& str)
        {
            std::vector<std::string> els = split(str, '=');
            return els[1];
        };

        std::ifstream in(metricsFilePath);

        // read header lines
        std::string infoline;
        std::getline(in, infoline);
        std::vector<std::string> infoelements = split(infoline, ' ');
        std::string facename = stringValueFromPair(infoelements[1]);

        float fontPixelHeight = floatValueFromPair(infoelements[2]);

        // read common line
        std::string commonline;
        std::getline(in, commonline);
        std::vector<std::string> commonelements = split(commonline, ' ');

        float lineHeight = floatValueFromPair(commonelements[1]);
        float normalisedLineHeight = lineHeight / fontPixelHeight;

        float baseLine = floatValueFromPair(commonelements[2]);
        float normalisedBaseLine = baseLine / fontPixelHeight;
        float scaleWidth = floatValueFromPair(commonelements[3]);
        float scaleHeight = floatValueFromPair(commonelements[4]);

        // read page id line
        std::string pageline;
        std::getline(in, pageline);

        auto pagelineelements = split(pageline, ' ');
        auto font_textureFile = stringValueFromPair(pagelineelements[2]);
        auto textureFilePath = vsg::concatPaths(vsg::filePath(metricsFilePath), font_textureFile.substr(1, font_textureFile.size()-2));

        font->atlas = vsg::read_cast<vsg::Data>(textureFilePath);
        if (!font->atlas)
        {
            std::cout<<"Could not read texture file : "<<font_textureFile<<std::endl;
            return {};
        }

        vsg::ref_ptr<vsg::ubyteArray2D> greyscaleImage = font->atlas.cast<vsg::ubyteArray2D>();
        if (!greyscaleImage)
        {
            greyscaleImage = vsg::ubyteArray2D::create(font->atlas->width(), font->atlas->height(), vsg::Data::Layout{VK_FORMAT_R8_UNORM});

            auto rgbaImage = font->atlas.cast<vsg::ubvec4Array2D>();
            if (rgbaImage)
            {
                auto dest_itr = greyscaleImage->begin();
                for(auto& value : *rgbaImage)
                {
                    (*dest_itr++) = value.a;
                }
                font->atlas = greyscaleImage;
            }

        }

        font->fontHeight = fontPixelHeight;
        font->normalisedLineHeight = normalisedLineHeight;

        // read character count line
        std::string charsline;
        std::getline(in, charsline);
        std::vector<std::string> charselements = split(charsline, ' ');
        uint32_t charscount = uintValueFromPair(charselements[1]);

        // read character data lines
        for (uint32_t i = 0; i < charscount; i++)
        {
            std::string line;
            std::getline(in, line);
            std::vector<std::string> elements = split(line, ' ');

            vsg::Font::GlyphMetrics glyph;

            glyph.charcode = uintValueFromPair(elements[1]);

            // pixel rect of glyph
            float x = floatValueFromPair(elements[2]);
            float y = floatValueFromPair(elements[3]);
            float width = floatValueFromPair(elements[4]);
            float height = floatValueFromPair(elements[5]);

            // adujst y to bottom origin
            y = scaleHeight - (y + height);

            // offset for character glyph in a string
            float xoffset = floatValueFromPair(elements[6]);
            float yoffset = floatValueFromPair(elements[7]);
            float xadvance = floatValueFromPair(elements[8]);

            // calc uv space rect
            vsg::vec2 uvorigin = vsg::vec2(x / scaleWidth, y / scaleHeight);
            vsg::vec2 uvsize = vsg::vec2(width / scaleWidth, height / scaleHeight);
            glyph.uvrect = vsg::vec4(uvorigin.x, uvorigin.y, uvorigin.x + uvsize.x, uvorigin.y + uvsize.y);

            // calc normaised size
            glyph.width = width / fontPixelHeight;
            glyph.height = height / fontPixelHeight;

            // calc normalise offsets
            glyph.horiBearingX = xoffset / fontPixelHeight;
            glyph.horiBearingY = normalisedBaseLine - (yoffset / fontPixelHeight);
            glyph.horiAdvance = xadvance / fontPixelHeight;

            glyph.vertBearingX = 0.0;
            glyph.vertBearingY = 0.0;
            glyph.vertAdvance = 1.0;


            // add glyph to list
            font->glyphs[glyph.charcode] = glyph;
        }
        return font;
    }
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    auto font_filename = arguments.value(std::string(), "-f");
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

    vsg::ref_ptr<vsg::Font> font;
    if (!font_filename.empty()) font = vsg::read_cast<vsg::Font>(font_filename, options);

    if (!font)
    {
        std::cout<<"Failling back to Unit3D roboto font,"<<std::endl;
        font = text::readUnity3dFontMetaFile("fonts/roboto.txt", options);
    }

    if (!font) return 1;

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

    std::cout<<"\ncomputeBounds.bounds.min = "<<computeBounds.bounds.min<<std::endl;
    std::cout<<"computeBounds.bounds.max = "<<computeBounds.bounds.max<<std::endl;

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
