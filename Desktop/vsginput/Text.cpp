#include "Text.h"

#include <iostream>
#include <sstream>

using namespace vsg;

TextGroup::TextGroup(Paths searchPaths, Allocator* allocator) :
    Inherit(allocator)
{
    // load shaders
    ref_ptr<ShaderModule> vertexShader = ShaderModule::read(VK_SHADER_STAGE_VERTEX_BIT, "main", findFile("shaders/vert_text.spv", searchPaths));
    ref_ptr<ShaderModule> fragmentShader = ShaderModule::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", findFile("shaders/frag_text.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return;
    }

    // set up graphics pipeline
    DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr }
    };

    DescriptorSetLayouts descriptorSetLayouts{ DescriptorSetLayout::create(descriptorBindings) };

    PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
    };

    VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data

        // per instance
        VkVertexInputBindingDescription{1, sizeof(GlyphInstanceData), VK_VERTEX_INPUT_RATE_INSTANCE} // glyph data
    };

    VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data

        // per instance Glyph data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32_SFLOAT, 0}, // instance offset (x,y)
        VkVertexInputAttributeDescription{2, 1, VK_FORMAT_R32_SFLOAT, sizeof(vec2)}, // glyph index in lookup textures
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

    ref_ptr<DepthStencilState> depthStencilState = DepthStencilState::create();
    depthStencilState->depthTestEnable = VK_TRUE;
    depthStencilState->depthWriteEnable = VK_TRUE;

    GraphicsPipelineStates pipelineStates
    {
        ShaderStages::create(ShaderModules{vertexShader, fragmentShader}),
        VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        InputAssemblyState::create(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP),
        RasterizationState::create(),
        MultisampleState::create(),
        ColorBlendState::create(colorBlendAttachments),
        depthStencilState
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

    _atlasTexture = Texture::create();
    _atlasTexture->_textureData = textureData;
    _atlasTexture->_dstBinding = 0;

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
    std::vector<std::string> charselements = split(charsline, ' ');
    uint32_t charscount = uintValueFromPair(charselements[1]);

    // allocate lookup texture data
    vsg::ref_ptr<vsg::vec4Array> uvTexels(new vsg::vec4Array(charscount));
    uvTexels->setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);

    vsg::ref_ptr<vsg::vec4Array> sizeTexels(new vsg::vec4Array(charscount));
    sizeTexels->setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);

    float lookupTexelSize = 1.0f / (float)charscount;
    float loopupTexelHalfSize = lookupTexelSize * 0.5f;

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
        y = _scaleHeight - (y + height);

        // offset for character glyph in a string
        float xoffset = floatValueFromPair(elements[6]);
        float yoffset = floatValueFromPair(elements[7]);
        float xadvance = floatValueFromPair(elements[8]);

        // calc uv space rect
        vec2 uvorigin = vec2(x / _scaleWidth, y / _scaleHeight);
        vec2 uvsize = vec2(width / _scaleWidth, height / _scaleHeight);
        glyph.uvrect = vec4(uvorigin.x, uvorigin.y, uvsize.x, uvsize.y);
       
        // calc normaised size
        glyph.size = vec2(width / _fontHeight, height / _fontHeight);

        // calc normalise offsets
        glyph.offset = vec2(xoffset / _fontHeight, _normalisedBaseLine - glyph.size.y - (yoffset / _fontHeight));
        glyph.xadvance = xadvance / _fontHeight;

        // calc offset into lookup texture
        glyph.lookupOffset = (lookupTexelSize * i) + loopupTexelHalfSize;

        // add glyph to list
        _glyphs[glyph.character] = glyph;

        // add data to lookup
        uvTexels->set(i, glyph.uvrect);
        sizeTexels->set(i, vec4(glyph.size.x, glyph.size.y, glyph.offset.x, glyph.offset.y));
    }

    _glyphUVsTexture = Texture::create();
    _glyphUVsTexture->_textureData = uvTexels;
    _glyphUVsTexture->_dstBinding = 1;

    _glyphSizesTexture = Texture::create();
    _glyphSizesTexture->_textureData = sizeTexels;
    _glyphSizesTexture->_dstBinding = 2;
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

    // create font metrics uniform
    _textMetrics = vsg::TextMetricsValue::create();
    _textMetrics->value().height = _fontHeight;
    _textMetrics->value().lineHeight = _font->_lineHeight;

    _textMetricsUniform = vsg::Uniform::create();
    _textMetricsUniform->_dataList.push_back(_textMetrics);
    _textMetricsUniform->_dstBinding = 3;

    // create and bind descriptorset for font texture
    auto graphicsPipeline = group->_bindGraphicsPipeline->getPipeline();
    auto& descriptorSetLayouts = graphicsPipeline->getPipelineLayout()->getDescriptorSetLayouts();

    auto descriptorSet = DescriptorSet::create(descriptorSetLayouts, Descriptors{ _font->_atlasTexture, _font->_glyphUVsTexture, _font->_glyphSizesTexture,  _textMetricsUniform });
    auto bindDescriptorSets = BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, DescriptorSets{ descriptorSet });

    add(bindDescriptorSets);
}

void Text::setText(const std::string& text)
{
    _text = text;

    // remove old children
    uint32_t childcount = static_cast<uint32_t>(_transform->getNumChildren());
    for(uint32_t i=0; i<childcount; i++)
    {
        _transform->removeChild(0);
    }

    //_transform->addChild(createQuadGlyphs(_text));
    _transform->addChild(createInstancedGlyphs(_text));
}

void Text::setFontHeight(const float& fontHeight)
{ 
    _fontHeight = fontHeight;
    _textMetrics->value().height = fontHeight;
    _textMetricsUniform->copyDataListToBuffers();
}

void Text::setPosition(const vec3& position)
{
    _transform->setMatrix(translate(position));
}

ref_ptr<Geometry> Text::createInstancedGlyphs(const std::string& text)
{
    bool useindices = true;

    // set up vertex and index arrays
    ref_ptr<vec3Array> vertices(new vec3Array
    {
        vec3(0.0f, 1.0f, 0.0f),
        vec3(0.0f, 0.0f, 0.0f),
        vec3(1.0f, 1.0f, 0.0f),
        vec3(1.0f, 0.0f, 0.0f)
    });

    vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray
    {
        0, 1, 2,
        2, 3, 1
    });

    // build the instance data
    uint32_t charcount = static_cast<uint32_t>(text.size());

    // this is a bit wrong at the mo, their might not be charcount instances as we skip new line and space characters
    ref_ptr<GlyphInstanceDataArray> instancedata(new GlyphInstanceDataArray(charcount));

    float x = 0.0f;
    float y = 0.0f;
    uint32_t instanceCount = 0;

    for (uint32_t i = 0; i < charcount; i++)
    {
        uint16_t character = (uint16_t)text.at(i);
        Font::GlyphData& glyphData = _font->_glyphs[character];

        if (character == '\n')
        {
            y -= _font->_normalisedLineHeight;
            x = 0.0f;
            continue;
        }
        else if (character == ' ')
        {
            x += glyphData.xadvance;
            continue;
        }

        GlyphInstanceData data;
        data.offset = vec2(x, y);
        data.lookupOffset = glyphData.lookupOffset;

        instancedata->set(instanceCount, data);
        instanceCount++;

        x += glyphData.xadvance;
    }

    // setup geometry
    auto geometry = Geometry::create();
    geometry->_arrays = DataList{ vertices, instancedata }; // for now stash the instance data in the arrays (share a buffer)
    if(useindices)
    {
        geometry->_indices = indices;
        geometry->_commands = Geometry::Commands{ DrawIndexed::create(6, instanceCount, 0, 0, 0) }; // draw char count instances
    }
    else
    {
        geometry->_commands = Geometry::Commands{ Draw::create(4, instanceCount, 0, 0) }; // draw char count instances
    }

    return geometry;
}