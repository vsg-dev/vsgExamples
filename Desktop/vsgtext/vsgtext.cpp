#include <vsg/all.h>
#include <iostream>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

#include "Font.h"
#include "Text.h"

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

            vsg::Font::GlyphData glyph;

            glyph.character = uintValueFromPair(elements[1]);

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
            glyph.uvrect = vsg::vec4(uvorigin.x, uvorigin.y, uvsize.x, uvsize.y);

            // calc normaised size
            glyph.size = vsg::vec2(width / fontPixelHeight, height / fontPixelHeight);

            // calc normalise offsets
            glyph.offset = vsg::vec2(xoffset / fontPixelHeight, normalisedBaseLine - glyph.size.y - (yoffset / fontPixelHeight));
            glyph.xadvance = xadvance / fontPixelHeight;

            // (the font object will calc this)
            glyph.lookupOffset = 0.0f;

            // add glyph to list
            font->glyphs[glyph.character] = glyph;
        }
        return font;
    }

    vsg::ref_ptr<vsg::Node> createText(const vsg::vec3& position, const std::string& text, vsg::ref_ptr<vsg::Font> font)
    {
        auto technique = font->getTechnique<vsg::StandardText>();
        if (!technique) return {};


        // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
        auto scenegraph = vsg::StateGroup::create();

        scenegraph->add(technique->bindGraphicsPipeline);
        scenegraph->add(technique->bindDescriptorSet);

        uint32_t num_quads = 0;
        uint32_t num_rows = 1;
        for(auto& character : text)
        {
            if (character == '\n') ++num_rows;
            else if (character != ' ') ++num_quads;
        }

        // check if we have anything to render
        if (num_quads==0) return {};

        uint32_t num_vertices = num_quads * 4;
        auto vertices = vsg::vec3Array::create(num_vertices);
        auto texcoords = vsg::vec2Array::create(num_vertices);
        auto colors = vsg::vec3Array::create(num_vertices);
        auto indices = vsg::ushortArray::create(num_quads * 6);

        uint32_t i = 0;
        uint32_t vi = 0;

        vsg::vec3 color(1.0f, 1.0f, 1.0f);
        vsg::vec3 row_position = position;
        vsg::vec3 pen_position = row_position;
        float row_height = 1.0;
        for(auto& character : text)
        {
            if (character == '\n')
            {
                // newline
                row_position.y -= font->normalisedLineHeight;
                pen_position = row_position;
            }
            else if (character == ' ')
            {
                // space
                uint16_t charcode(character);
                const auto& glyph = font->glyphs[charcode];
                pen_position.x += glyph.xadvance;
            }
            else
            {
                uint16_t charcode(character);
                const auto& glyph = font->glyphs[charcode];

                const auto& uvrect = glyph.uvrect;

                vsg::vec3 local_origin = pen_position + vsg::vec3(glyph.offset.x, glyph.offset.y, 0.0f);
                const auto& size = glyph.size;

                vertices->set(vi, local_origin);
                vertices->set(vi+1, local_origin + vsg::vec3(size.x, 0.0f, 0.0f));
                vertices->set(vi+2, local_origin + vsg::vec3(size.x, size.y, 0.0f));
                vertices->set(vi+3, local_origin + vsg::vec3(0.0f, size.y, 0.0f));

                colors->at(vi  ) = color;
                colors->at(vi+1) = color;
                colors->at(vi+2) = color;
                colors->at(vi+3) = color;

                texcoords->set(vi, vsg::vec2(uvrect[0], uvrect[1]));
                texcoords->set(vi+1, vsg::vec2(uvrect[0]+uvrect[2], uvrect[1]));
                texcoords->set(vi+2, vsg::vec2(uvrect[0]+uvrect[2], uvrect[1]+uvrect[3]));
                texcoords->set(vi+3, vsg::vec2(uvrect[0], uvrect[1]+uvrect[3]));

                indices->set(i, vi);
                indices->set(i+1, vi+1);
                indices->set(i+2, vi+2);
                indices->set(i+3, vi+2);
                indices->set(i+4, vi+3);
                indices->set(i+5, vi);

                vi += 4;
                i += 6;
                pen_position.x += glyph.xadvance;
            }
        }

        // setup geometry
        auto drawCommands = vsg::Commands::create();
        drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
        drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
        drawCommands->addChild(vsg::DrawIndexed::create(indices->size(), 1, 0, 0, 0));

        scenegraph->addChild(drawCommands);

        return scenegraph;
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

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    auto options = vsg::Options::create();
    options->paths = searchPaths;
    #ifdef USE_VSGXCHANGE
    options->readerWriter = vsgXchange::ReaderWriter_all::create();
    #endif

    auto font = text::readUnity3dFontMetaFile("fonts/roboto.txt", options);
    std::cout<<"font = "<<font<<std::endl;

    if (!font) return 1;

    // set up model transformation node
    auto scenegraph = vsg::Group::create();

    scenegraph->addChild(text::createText(vsg::vec3(0.0, 0.0, 0.0), "This is my text\nTest another line", font));

    vsg::write(scenegraph, "text.vsgt");

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
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 5.5);
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, radius*3.5), centre, vsg::dvec3(0.0, 0.0, 1.0));
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
