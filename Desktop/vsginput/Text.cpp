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

    auto valueFromPair = [&split](const std::string& str)
    {
        std::vector<std::string> els = split(str, '=');
        return atoi(els[1].c_str());
    };

    std::string dataFile("fonts/" + fontname + ".txt");
    std::ifstream in(findFile(dataFile, searchPaths));

    // read header lines
    std::string infoline;
    std::getline(in, infoline);

    std::string commonline;
    std::getline(in, commonline);
    std::vector<std::string> commonelements = split(commonline, ' ');
    _lineHeight = valueFromPair(commonelements[1]);
    _base = valueFromPair(commonelements[2]);
    _scaleWidth = valueFromPair(commonelements[3]);
    _scaleHeight = valueFromPair(commonelements[4]);

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

        glyph.character = valueFromPair(elements[1]);
        glyph.x = valueFromPair(elements[2]);
        glyph.y = valueFromPair(elements[3]);
        glyph.width = valueFromPair(elements[4]);
        glyph.height = valueFromPair(elements[5]);
        glyph.xoffset = valueFromPair(elements[6]);
        glyph.yoffset = valueFromPair(elements[7]);
        glyph.xadvance = valueFromPair(elements[8]);

        glyph.uvOrigin = vec2((float)glyph.x / (float)_scaleWidth, (float)glyph.y / (float)_scaleHeight);
        glyph.uvSize = vec2((float)glyph.width / (float)_scaleWidth, (float)glyph.height / (float)_scaleHeight);

        _glyphs[glyph.character] = glyph;
    }
}

//
// Text
//

Text::Text(Font* font, TextGroup* group, Allocator* allocator) :
    Inherit(allocator)
{
    _font = font;

    // create and bind descriptorset for font texture
    auto graphicsPipeline = group->_bindGraphicsPipeline->getPipeline();
    auto& descriptorSetLayouts = graphicsPipeline->getPipelineLayout()->getDescriptorSetLayouts();

    auto descriptorSet = DescriptorSet::create(descriptorSetLayouts, Descriptors{ _font->_texture });
    auto bindDescriptorSets = BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, DescriptorSets{ descriptorSet });

    add(bindDescriptorSets);

    // add a default quad
//    ref_ptr<Geometry> glyph = createGlyphQuad(vec3(0.0f,0.0f,0.0f), vec3(1.0f,0.0f,0.0f), vec3(0.0f,1.0f,0.0f), 0.0f, 0.0f, 1.0f, 1.0f);
//    addChild(glyph);

    setText("Cheese!");
}

void Text::setText(const std::string& text)
{
    // remove old children
    uint32_t childcount = getNumChildren();
    for(uint32_t i=0; i<childcount; i++)
    {
        removeChild(0);
    }

    float x = 0.0f;
    for (int i = 0; i < text.size(); i++)
    {
        uint16_t character = (uint16_t)text.at(i);
        // see if we have glyph data
        if(_font->_glyphs.find(character) == _font->_glyphs.end()) continue;

        Font::GlyphData glyphData = _font->_glyphs[character];

        ref_ptr<Geometry> glyph = createGlyphQuad(vec3(x, 0.0f, 0.0f), vec3(glyphData.width/50.0f, 0.0f, 0.0f), vec3(0.0f, glyphData.height/50.0f, 0.0f), glyphData.uvOrigin.x, 1.0f - glyphData.uvOrigin.y, glyphData.uvOrigin.x + glyphData.uvSize.x, 1.0f - (glyphData.uvOrigin.y + glyphData.uvSize.y));
        addChild(glyph);

        x += glyphData.xadvance/50.0f;
    }
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
