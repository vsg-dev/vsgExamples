#include <vsg/all.h>
#include <iostream>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

namespace text
{
    struct GlyphData
    {
        uint16_t character;
        vsg::vec4 uvrect; // min x/y, max x/y
        vsg::vec2 size; // normalised size of the glyph
        vsg::vec2 offset; // normalised offset
        float xadvance; // normalised xadvance
        float lookupOffset; // offset into lookup texture
    };
    using GlyphMap = std::map<uint16_t, GlyphData>;

    bool readUnity3dFontMetaFile(const std::string& filePath, GlyphMap& glyphMap, float& fontPixelHeight, float& normalisedLineHeight)
    {
        std::cout<<"filePath = "<<filePath<<std::endl;

        if (filePath.empty()) return false;

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


        std::ifstream in(filePath);

        // read header lines
        std::string infoline;
        std::getline(in, infoline);
        std::vector<std::string> infoelements = split(infoline, ' ');
        std::string facename = stringValueFromPair(infoelements[1]);

        fontPixelHeight = floatValueFromPair(infoelements[2]);

        // read common line
        std::string commonline;
        std::getline(in, commonline);
        std::vector<std::string> commonelements = split(commonline, ' ');

        float lineHeight = floatValueFromPair(commonelements[1]);
        normalisedLineHeight = lineHeight / fontPixelHeight;

        float baseLine = floatValueFromPair(commonelements[2]);
        float normalisedBaseLine = baseLine / fontPixelHeight;
        float scaleWidth = floatValueFromPair(commonelements[3]);
        float scaleHeight = floatValueFromPair(commonelements[4]);

        // read page id line
        std::string pageline;
        std::getline(in, pageline);

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

            GlyphData glyph;

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
            glyphMap[glyph.character] = glyph;
        }
        return true;
    }

    class Font : public vsg::Inherit<vsg::Object, Font>
    {
    public:
        vsg::ref_ptr<vsg::Data> atlas;
        vsg::ref_ptr<vsg::DescriptorImage> texture;
        GlyphMap glyphs;
        float fontHeight;
        float normalisedLineHeight;
        vsg::ref_ptr<vsg::Options> options;

        /// Technique base class provide ability to provide range of different rendering techniques
        class Technique : public vsg::Inherit<vsg::Object, Technique>
        {
        public:
            vsg::ref_ptr<vsg::BindGraphicsPipeline> bindGraphicsPipeline;
            vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
        };

        std::vector<vsg::ref_ptr<Technique>> techniques;

        /// get or create a Technique instance that matches the specified type
        template<class T>
        vsg::ref_ptr<T> getTechnique()
        {
            for(auto& technique : techniques)
            {
                auto tech = technique.cast<T>();
                if (tech) return tech;
            }
            auto tech = T::create(this);
            techniques.emplace_back(tech);
            return tech;
        }
    };

    class StandardText : public vsg::Inherit<Font::Technique, StandardText>
    {
    public:
        StandardText(Font* font)
        {
            auto textureData = font->atlas;

            // load shaders
            auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/text.vert", font->options);
            auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/text.frag", font->options);

            std::cout<<"vertexShader = "<<vertexShader<<std::endl;
            std::cout<<"fragmentShader = "<<fragmentShader<<std::endl;

            if (!vertexShader || !fragmentShader)
            {
                std::cout<<"Could not create shaders."<<std::endl;
            }

    #ifdef USE_VSGXCHANGE
            // compile section
            vsg::ShaderStages stagesToCompile;
            if (vertexShader && vertexShader->module && vertexShader->module->code.empty()) stagesToCompile.emplace_back(vertexShader);
            if (fragmentShader && fragmentShader->module && fragmentShader->module->code.empty()) stagesToCompile.emplace_back(fragmentShader);

            if (!stagesToCompile.empty())
            {
                auto shaderCompiler = vsgXchange::ShaderCompiler::create();

                std::vector<std::string> defines;
                shaderCompiler->compile(stagesToCompile, defines); // and paths?
            }
            // TODO end of block requiring changes
    #endif

            // set up graphics pipeline
            vsg::DescriptorSetLayoutBindings descriptorBindings
            {
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
            };

            auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

            vsg::PushConstantRanges pushConstantRanges
            {
                {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
            };

            vsg::VertexInputState::Bindings vertexBindingsDescriptions
            {
                VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
                VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
                VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
            };

            vsg::VertexInputState::Attributes vertexAttributeDescriptions
            {
                VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
                VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
                VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
            };

            // alpha blending
            VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;

            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            auto blending = vsg::ColorBlendState::create(vsg::ColorBlendState::ColorBlendAttachments{colorBlendAttachment});

            // switch off back face culling
            auto rasterization = vsg::RasterizationState::create();
            rasterization->cullMode = VK_CULL_MODE_NONE;

            vsg::GraphicsPipelineStates pipelineStates
            {
                vsg::VertexInputState::create( vertexBindingsDescriptions, vertexAttributeDescriptions ),
                vsg::InputAssemblyState::create(),
                vsg::MultisampleState::create(),
                blending,
                rasterization,
                vsg::DepthStencilState::create()
            };

            auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
            auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
            bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

            // create texture image and associated DescriptorSets and binding
            auto texture = vsg::DescriptorImage::create(vsg::Sampler::create(), textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});

            bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);
        }
    };


    class Text : public vsg::Inherit<vsg::Node, Text>
    {
    public:
        vsg::ref_ptr<Font> font;
        vsg::ref_ptr<Font::Technique> technique;
        vsg::vec3 position;
        std::string text;
    };

    vsg::ref_ptr<Font> readFont(const std::string& fontName, vsg::ref_ptr<vsg::Options> options)
    {

        vsg::Path font_textureFile(vsg::concatPaths("fonts", fontName) + ".vsgb");
        vsg::Path font_metricsFile(vsg::concatPaths("fonts", fontName) + ".txt");

        auto font = Font::create();

        font->options = options;

        font->atlas = vsg::read_cast<vsg::Data>(vsg::findFile(font_textureFile, options->paths));
        if (!font->atlas)
        {
            std::cout<<"Could not read texture file : "<<font_textureFile<<std::endl;
            return {};
        }

        if (!text::readUnity3dFontMetaFile(vsg::findFile(font_metricsFile, options->paths), font->glyphs, font->fontHeight, font->normalisedLineHeight))
        {
            std::cout<<"Could not reading font metrics file"<<std::endl;
            return {};
        }

        // read texture image
        return font;
    }

    vsg::ref_ptr<vsg::Node> createText(const vsg::vec3& position, const std::string& text, vsg::ref_ptr<Font> font)
    {
        auto technique = font->getTechnique<text::StandardText>();
        std::cout<<"technique = "<<technique<<std::endl;
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
                std::cout<<"   NEWLINE"<<std::endl;

                row_position.y -= font->normalisedLineHeight;
                pen_position = row_position;
            }
            else if (character == ' ')
            {
                // space
                std::cout<<"   SPACE"<<std::endl;
                uint16_t charcode(character);
                const auto& glyph = font->glyphs[charcode];
                pen_position.x += glyph.xadvance;
            }
            else
            {
                uint16_t charcode(character);
                const auto& glyph = font->glyphs[charcode];
#if 0
                vsg::vec4 uvrect; // min x/y, max x/y
                vsg::vec2 size; // normalised size of the glyph
                vsg::vec2 offset; // normalised offset
                float xadvance; // normalised xadvance
                float lookupOffset; // offset into lookup texture
#endif
                std::cout<<"   Charcode = "<<charcode<<", "<<character<<", glyph.uvrect = ["<<glyph.uvrect<<"], glyph.offset = "<<glyph.offset<<", glyph.xadvance = "<<glyph.xadvance<<std::endl;

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

#if 0
                texcoords->set(vi, vsg::vec2(uvrect[0], uvrect[2]));
                texcoords->set(vi+1, vsg::vec2(uvrect[1], uvrect[2]));
                texcoords->set(vi+2, vsg::vec2(uvrect[1], uvrect[3]));
                texcoords->set(vi+3, vsg::vec2(uvrect[0], uvrect[3]));
#else
                texcoords->set(vi, vsg::vec2(uvrect[0], uvrect[1]));
                texcoords->set(vi+1, vsg::vec2(uvrect[0]+uvrect[2], uvrect[1]));
                texcoords->set(vi+2, vsg::vec2(uvrect[0]+uvrect[2], uvrect[1]+uvrect[3]));
                texcoords->set(vi+3, vsg::vec2(uvrect[0], uvrect[1]+uvrect[3]));
#endif
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

    auto font = text::readFont("roboto", options);
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


    std::cout<<"\n centre = "<<centre<<std::endl;
    std::cout<<"\n radius = "<<radius<<std::endl;

    // set up the camera
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(radius*3.5, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
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
