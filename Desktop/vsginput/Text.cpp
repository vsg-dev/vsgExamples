#include "Text.h"

#include <iostream>
#include <sstream>

using namespace vsg;

TextGroup::TextGroup(Paths searchPaths, Allocator* allocator) :
    Inherit(allocator)
{
    // load shaders
    ref_ptr<ShaderModule> vertexShader = ShaderModule::read(VK_SHADER_STAGE_VERTEX_BIT, "main", findFile("shaders/vert_PushConstants.spv", searchPaths));
    ref_ptr<ShaderModule> fragmentShader = ShaderModule::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return;
    }

    // set up graphics pipeline
    DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    DescriptorSetLayouts descriptorSetLayouts{ DescriptorSetLayout::create(descriptorBindings) };

    PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
    };

    VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vec3), VK_VERTEX_INPUT_RATE_INSTANCE}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    // setup blending
    vsg::ColorBlendState::ColorBlendAttachments colorBlendAttachments;
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

    colorBlendAttachments.push_back(colorBlendAttachment);

    GraphicsPipelineStates pipelineStates
    {
        ShaderStages::create(ShaderModules{vertexShader, fragmentShader}),
        VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        InputAssemblyState::create(),
        RasterizationState::create(),
        MultisampleState::create(),
        ColorBlendState::create(colorBlendAttachments),
        DepthStencilState::create()
    };

    auto pipelineLayout = PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);
    auto graphicsPipeline = GraphicsPipeline::create(pipelineLayout, pipelineStates);
    _bindGraphicsPipeline = BindGraphicsPipeline::create(graphicsPipeline);
    add(_bindGraphicsPipeline);
}

//
// Font
//

Font::Font(const std::string& fontname, Paths searchPaths, Allocator* allocator) :
    Inherit(allocator)
{
    // load glyph atlas
    std::string textureFile("fonts/" + fontname + ".vsgb");
    vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<Data>(findFile(textureFile, searchPaths));
    if (!textureData)
    {
        std::cout << "Could not read font texture file : " << textureFile << std::endl;
        return;
    }

    _texture = Texture::create();
    _texture->_textureData = textureData;
    _texture->_dstBinding = 0;

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

    std::string dataFile("fonts/" + fontname + ".txt");
    std::ifstream in(findFile(dataFile, searchPaths));

    // read header lines
    std::string infoline;
    std::getline(in, infoline);
    std::vector<std::string> infoelements = split(infoline, ' ');
    std::string facename = stringValueFromPair(infoelements[1]);
    _fontHeight = floatValueFromPair(infoelements[2]);

    std::string commonline;
    std::getline(in, commonline);
    std::vector<std::string> commonelements = split(commonline, ' ');
    _lineHeight = floatValueFromPair(commonelements[1]);
    _normalisedLineHeight = _lineHeight / _fontHeight;
    _baseLine = floatValueFromPair(commonelements[2]);
    _normalisedBaseLine = _baseLine / _fontHeight;
    _scaleWidth = floatValueFromPair(commonelements[3]);
    _scaleHeight = floatValueFromPair(commonelements[4]);

    std::string pageline;
    std::getline(in, pageline);

    std::string charsline;
    std::getline(in, charsline);

    for (std::string line; std::getline(in, line); )
    {
        std::cout << line << std::endl;
        if(!startsWith(line, "char id")) continue;

        std::vector<std::string> elements = split(line, ' ');

        GlyphData glyph;

        glyph.character = uintValueFromPair(elements[1]);

        // pixel rect of glyph
        float x = floatValueFromPair(elements[2]);
        float y = floatValueFromPair(elements[3]);
        float width = floatValueFromPair(elements[4]);
        float height = floatValueFromPair(elements[5]);

        // adujst y to bottom origin
        y = _scaleHeight - (y + height);

        // offset for character glyph in a string
        float xoffset = floatValueFromPair(elements[6]);
        float yoffset = floatValueFromPair(elements[7]);
        float xadvance = floatValueFromPair(elements[8]);

        // calc uv space rect
        vec2 uvorigin = vec2(x / _scaleWidth, y / _scaleHeight);
        vec2 uvsize = vec2(width / _scaleWidth, height / _scaleHeight);
        glyph.uvrect = vec4(uvorigin.x, uvorigin.y, uvorigin.x + uvsize.x, uvorigin.y + uvsize.y);
        glyph.uvrect = vec4(uvorigin.x, uvorigin.y, uvorigin.x + uvsize.x, uvorigin.y + uvsize.y);
       
        // calc normaised size
        glyph.size = vec2(width / _fontHeight, height / _fontHeight);

        // calc normalise offsets
        glyph.offset = vec2(xoffset / _fontHeight, _normalisedBaseLine - glyph.size.y - (yoffset / _fontHeight));
        glyph.xadvance = xadvance / _fontHeight;

        _glyphs[glyph.character] = glyph;
    }
}

//
// Text
//

Text::Text(Font* font, TextGroup* group, Allocator* allocator) :
    Inherit(allocator),
    _fontHeight(30.0f)
{
    _font = font;

    // create transform to translate text for now
    _transform = MatrixTransform::create();
    addChild(_transform);

    // create and bind descriptorset for font texture
    auto graphicsPipeline = group->_bindGraphicsPipeline->getPipeline();
    auto& descriptorSetLayouts = graphicsPipeline->getPipelineLayout()->getDescriptorSetLayouts();

    auto descriptorSet = DescriptorSet::create(descriptorSetLayouts, Descriptors{ _font->_texture });
    auto bindDescriptorSets = BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, DescriptorSets{ descriptorSet });

    add(bindDescriptorSets);
}

void Text::setText(const std::string& text)
{
    _text = text;

    // remove old children
    uint32_t childcount = _transform->getNumChildren();
    for(uint32_t i=0; i<childcount; i++)
    {
        _transform->removeChild(0);
    }

    float x = 0.0f;
    float y = 0.0f;
    for (int i = 0; i < text.size(); i++)
    {
        uint16_t character = (uint16_t)text.at(i);

        if (character == '\n')
        {
            y -= _font->_normalisedLineHeight * _fontHeight;
            x = 0.0f;
            continue;
        }

        // see if we have glyph data
        if(_font->_glyphs.find(character) == _font->_glyphs.end())
        {
            // for now use @ symbol for missing char
            character = '@';
        }

        Font::GlyphData glyphData = _font->_glyphs[character];

        ref_ptr<Geometry> glyph = createGlyphQuad(vec3(x + (glyphData.offset.x * _fontHeight), y + (glyphData.offset.y * _fontHeight), 0.0f), vec3(glyphData.size.x * _fontHeight, 0.0f, 0.0f), vec3(0.0f, glyphData.size.y * _fontHeight, 0.0f), glyphData.uvrect.x, glyphData.uvrect.y, glyphData.uvrect.z, glyphData.uvrect.w);
        _transform->addChild(glyph);

        x += glyphData.xadvance * _fontHeight;
    }
}

void Text::setPosition(const vec3& position)
{
    _transform->setMatrix(translate(position));
}

ref_ptr<Geometry> Text::createGlyphQuad(const vec3& corner, const vec3& widthVec, const vec3& heightVec, float l, float b, float r, float t)
{
    // set up vertex and index arrays
    ref_ptr<vec3Array> vertices(new vec3Array
    {
        corner,
        corner + widthVec,
        corner + widthVec + heightVec,
        corner + heightVec
    });

    ref_ptr<vec3Array> colors(new vec3Array
    {
        {1.0f, 1.0f, 1.0f}
    });

    ref_ptr<vec2Array> texcoords(new vec2Array
    {
        {l, b},
        {r, b},
        {r, t},
        {l, t}
    });

    ref_ptr<ushortArray> indices(new ushortArray
    {
        0, 1, 2,
        2, 3, 0
    });

    // setup geometry
    auto geometry = Geometry::create();
    geometry->_arrays = DataList{ vertices, colors, texcoords };
    geometry->_indices = indices;
    geometry->_commands = Geometry::Commands{ DrawIndexed::create(6, 1, 0, 0, 0) };

    return geometry;
}
