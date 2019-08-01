#include "Text.h"

#include <iostream>
#include <sstream>

using namespace vsg;

GraphicsPipelineBuilder::GraphicsPipelineBuilder(Paths searchPaths, Allocator* allocator) :
    Inherit(allocator)
{
}
   
void GraphicsPipelineBuilder::build(vsg::ref_ptr<Traits> traits)
{
    // set up graphics pipeline

    // create descriptor layouts
    DescriptorSetLayouts descriptorSetLayouts;
    uint32_t bindingIndex = 0; // increment the binding index across all sets

    for (uint32_t i = 0; i < traits->descriptorLayouts.size(); i++)
    {
        Traits::BindingSet bindingSet = traits->descriptorLayouts[i];
        DescriptorSetLayoutBindings setLayoutBindings;
        for(VkShaderStageFlags stage : { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT } )
        {
            Traits::BindingTypes descriptorTypes = bindingSet[stage];
            for(uint32_t b = 0; b < descriptorTypes.size(); b++)
            {
                setLayoutBindings.push_back({ bindingIndex, descriptorTypes[b], 1, stage, nullptr });
                bindingIndex++;
            }
        }
        descriptorSetLayouts.push_back(DescriptorSetLayout::create(setLayoutBindings));
    }

    // create vertex bindings and attributes
    VertexInputState::Bindings vertexBindingsDescriptions;
    VertexInputState::Attributes vertexAttributeDescriptions;
    uint32_t locationIndex = 0;
    bindingIndex = 0;

    for (auto& rate : { VK_VERTEX_INPUT_RATE_VERTEX, VK_VERTEX_INPUT_RATE_INSTANCE })
    {
        Traits::BindingFormats rateFormats = traits->vertexAttributes[rate];

        for (uint32_t i = 0; i < rateFormats.size(); i++)
        {
            Traits::BindingFormat& format = rateFormats[i];
            // sum the size of this object
            uint32_t totalsize = static_cast<uint32_t>(sizeOf(format));

            vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ bindingIndex, totalsize, rate });

            uint32_t offset = 0;
            for (uint32_t l = 0; l < format.size(); l++)
            {
                vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ locationIndex, bindingIndex, format[l], offset });
                offset += static_cast<uint32_t>(sizeOf(format[l]));
                locationIndex++;
            }

            bindingIndex++;
        }
    }

    GraphicsPipelineStates pipelineStates
    {
        VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        InputAssemblyState::create(traits->primitiveTopology),
        RasterizationState::create(),
        MultisampleState::create(),
        traits->colorBlendAttachments.size() > 0 ? ColorBlendState::create(traits->colorBlendAttachments) : ColorBlendState::create(),
        DepthStencilState::create()
    };

    PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
    };


    auto pipelineLayout = PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);
    _graphicsPipeline = GraphicsPipeline::create(pipelineLayout, traits->shaderStages, pipelineStates);
}

size_t GraphicsPipelineBuilder::sizeOf(VkFormat format)
{
    switch(format)
    {
        // float
        case VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT: return sizeof(vec4);
        case VkFormat::VK_FORMAT_R32G32B32_SFLOAT: return sizeof(vec3);
        case VkFormat::VK_FORMAT_R32G32_SFLOAT: return sizeof(vec2);
        case VkFormat::VK_FORMAT_R32_SFLOAT: return sizeof(float);

        // uint8
        case VkFormat::VK_FORMAT_B8G8R8A8_UINT: return sizeof(ubvec4);
        case VkFormat::VK_FORMAT_B8G8R8_UINT: return sizeof(ubvec3);
        case VkFormat::VK_FORMAT_R8G8_UINT: return sizeof(ubvec2);
        case VkFormat::VK_FORMAT_R8_UINT: return sizeof(uint8_t);

        // uint16
        case VkFormat::VK_FORMAT_R16_UINT: return sizeof(uint16_t);

        // uint32
        case VkFormat::VK_FORMAT_R32_UINT: return sizeof(uint32_t);

        default: break;
    }
    return 0;
}
size_t GraphicsPipelineBuilder::sizeOf(const std::vector<VkFormat>& formats)
{
    size_t size = 0;
    for (auto& format : formats)
    {
        size += sizeOf(format);
    }
    return size;
}

//
// TextGraphicsPipelineBuilder
//
TextGraphicsPipelineBuilder::TextGraphicsPipelineBuilder(Paths searchPaths, Allocator* allocator) :
    Inherit(searchPaths, allocator)
{
    ref_ptr<Traits> traits = Traits::create();

    // vertex input
    traits->vertexAttributes[VK_VERTEX_INPUT_RATE_VERTEX] =
    {
        // vertex x,y,z
        { 
            VK_FORMAT_R32G32B32_SFLOAT
        }
    };

    traits->vertexAttributes[VK_VERTEX_INPUT_RATE_INSTANCE] =
    {
        // per glyph instance data position, offset, lookup offset
        {
            VK_FORMAT_R32G32B32_SFLOAT,
            VK_FORMAT_R32G32_SFLOAT,
            VK_FORMAT_R32_SFLOAT
        }
    };

    // descriptor sets layout
    Traits::BindingSet fontSet;
    fontSet[VK_SHADER_STAGE_VERTEX_BIT] = {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    };
    fontSet[VK_SHADER_STAGE_FRAGMENT_BIT] = {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    };

    Traits::BindingSet textSet;
    textSet[VK_SHADER_STAGE_VERTEX_BIT] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

    traits->descriptorLayouts =
    {
        fontSet,
        textSet
    };

    // shaders
    ref_ptr<ShaderStage> vertexShader = ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", findFile("shaders/vert_text.spv", searchPaths));
    ref_ptr<ShaderStage> fragmentShader = ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", findFile("shaders/frag_text.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return;
    }
    traits->shaderStages = { vertexShader, fragmentShader };

    // topology
    traits->primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

   // alpha blending
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

    traits->colorBlendAttachments.push_back(colorBlendAttachment);

    build(traits);
}

//
// Font
//

Font::Font(PipelineLayout* pipelineLayout, const std::string& fontname, Paths searchPaths) :
    Inherit(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, DescriptorSets{ })
{
    // load glyph atlas
    std::string textureFile("fonts/" + fontname + ".vsgb");
    ReaderWriter_vsg vsgReader;
    auto textureData = vsgReader.read_cast<Data>(findFile(textureFile, searchPaths));
    if (!textureData)
    {
        std::cout << "Could not read font texture file : " << textureFile << std::endl;
        return;
    }

    _atlasTexture = DescriptorImage::create(vsg::Sampler::create(), textureData, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    std::string fontFile = "fonts/" + fontname + ".txt";
    if (!readUnity3dFontMetaFile(findFile(fontFile, searchPaths), _glyphs, _fontHeight, _normalisedLineHeight))
    {
        std::cout << "Could not read font meta file : " << fontFile << std::endl;
        return;
    }

     // allocate lookup texture data
     uint32_t glyphs_size = static_cast<uint32_t>(_glyphs.size());
     auto uvTexels = vsg::vec4Array::create(glyphs_size);
     uvTexels->setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);

     auto sizeTexels = vsg::vec4Array::create(glyphs_size);
     sizeTexels->setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);

     float lookupTexelSize = 1.0f / (float)_glyphs.size();
     float loopupTexelHalfSize = lookupTexelSize * 0.5f;

     // fill lookup textures
     uint32_t i = 0;
     for(auto& glyph : _glyphs)
     {
        // calc offset into lookup texture
        glyph.second.lookupOffset = (lookupTexelSize * i) + loopupTexelHalfSize;

        // add data to lookup
        uvTexels->set(i, glyph.second.uvrect);
        sizeTexels->set(i, vec4(glyph.second.offset.x, glyph.second.offset.y, glyph.second.size.x, glyph.second.size.y));

        i++;
     }

    _glyphUVsTexture = DescriptorImage::create(vsg::Sampler::create(), uvTexels, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    _glyphSizesTexture = DescriptorImage::create(vsg::Sampler::create(), sizeTexels, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // create and bind descriptorset for font texture
    auto& descriptorSetLayouts = pipelineLayout->getDescriptorSetLayouts();

    DescriptorSetLayouts fontDescriptorSetLayouts =  { descriptorSetLayouts[0] };

    _descriptorSets = DescriptorSets{ DescriptorSet::create(fontDescriptorSetLayouts, Descriptors{ _glyphUVsTexture, _glyphSizesTexture, _atlasTexture }) };
}

//
// GlyphGeometry
//

GlyphGeometry::GlyphGeometry(Allocator* allocator) :
    Inherit(allocator)
{
}

void GlyphGeometry::compile(Context& context)
{
    if (!_renderImplementation.empty()) return;

    _renderImplementation.clear();

    bool failure = false;

    // set up vertex and index arrays
    auto vertices = vec3Array::create(
    {
        vec3(0.0f, 1.0f, 0.0f),
        vec3(0.0f, 0.0f, 0.0f),
        vec3(1.0f, 1.0f, 0.0f),
        vec3(1.0f, 0.0f, 0.0f)
    });

    auto indices = vsg::ushortArray::create(
    {
        0, 1, 2, 3
    });


    DataList dataList;
    dataList.reserve(3);
    dataList.emplace_back(vertices);
    dataList.emplace_back(_glyphInstances);
    dataList.emplace_back(indices);

    auto bufferData = vsg::createBufferAndTransferData(context, dataList, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    if (!bufferData.empty())
    {
        BufferDataList vertexBufferData(bufferData.begin(), bufferData.begin() + 2); //verts and glyph instances
        vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);
        if (bindVertexBuffers)
            _renderImplementation.emplace_back(bindVertexBuffers);
        else
            failure = true;

        vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(bufferData.back());
        if (bindIndexBuffer)
            _renderImplementation.emplace_back(bindIndexBuffer);
        else
            failure = true;
    }
    else
        failure = true;

    if (failure)
    {
        //std::cout<<"Failed to create required arrays/indices buffers on GPU."<<std::endl;
        _renderImplementation.clear();
        return;
    }

    // add the draw indexed command
    _renderImplementation.emplace_back(DrawIndexed::create(4, static_cast<uint32_t>(_glyphInstances->valueCount()), 0, 0, 0));
}

void GlyphGeometry::dispatch(CommandBuffer& commandBuffer) const
{
    for (auto& command : _renderImplementation)
    {
        command->dispatch(commandBuffer);
    }
}

//
// TextBase
//

TextBase::TextBase(Font* font, GraphicsPipeline* pipeline, Allocator* allocator) :
    Inherit(allocator)
{
    _font = font;

    // create transform to translate text for now
    _transform = MatrixTransform::create();
    addChild(_transform);

    // create font metrics uniform
    _textMetrics = vsg::TextMetricsValue::create();
    _textMetrics->value().height = 30.0f;
    _textMetrics->value().lineHeight = 1.0f;

    _textMetricsUniform = vsg::DescriptorBuffer::create(_textMetrics,3);

    // create and bind descriptorset for text metrix uniform
    auto& descriptorSetLayouts = pipeline->getPipelineLayout()->getDescriptorSetLayouts();

    DescriptorSetLayouts textDescriptorSetLayouts = { descriptorSetLayouts[1] };

    auto descriptorSet = DescriptorSet::create(textDescriptorSetLayouts, Descriptors{ _textMetricsUniform });
    auto bindDescriptorSets = BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 1, DescriptorSets{ descriptorSet });
    bindDescriptorSets->setSlot(2); // Font goes to slot 1, so use slot 2

    add(bindDescriptorSets);
}

void TextBase::setFont(Font* font)
{
    _font = font;
    buildTextGraph();
}

void TextBase::setFontHeight(const float& fontHeight)
{
    _textMetrics->value().height = fontHeight;
    _textMetricsUniform->copyDataListToBuffers();
}

void TextBase::setBillboardAxis(const vec3& billboardAxis)
{
    _textMetrics->value().billboardAxis = billboardAxis;
    _textMetricsUniform->copyDataListToBuffers();
}

void TextBase::setPosition(const vec3& position)
{
    _transform->setMatrix(translate(position));
}

void TextBase::buildTextGraph()
{
    // remove old children
    uint32_t childcount = static_cast<uint32_t>(_transform->getNumChildren());
    for (uint32_t i = 0; i < childcount; i++)
    {
        _transform->removeChild(0);
    }

    _transform->addChild(createInstancedGlyphs());
}

//
// Text
//

Text::Text(Font* font, GraphicsPipeline* pipeline, Allocator* allocator) :
    Inherit(font, pipeline, allocator)
{
}

void Text::setText(const std::string& text)
{
    _text = text;
    buildTextGraph();
}

ref_ptr<GlyphGeometry> Text::createInstancedGlyphs()
{
    // build the instance data
    uint32_t charcount = static_cast<uint32_t>(_text.size());

    // this is a bit wrong at the mo, their might not be charcount instances as we skip new line and space characters
    auto instancedata = GlyphInstanceDataArray::create(charcount);

    float x = 0.0f;
    float y = 0.0f;
    uint32_t instanceCount = 0;

    for (uint32_t i = 0; i < charcount; i++)
    {
        uint16_t character = (uint16_t)_text.at(i);
        const Font::GlyphData& glyphData = _font->getGlyphMap()[character];

        if (character == '\n')
        {
            y -= _font->getNormalisedLineHeight();
            x = 0.0f;
            continue;
        }
        else if (character == ' ')
        {
            x += glyphData.xadvance;
            continue;
        }

        GlyphInstanceData data;
        data.position = vec3(0.0f,0.0f,0.0f);
        data.offset = vec2(x, y);
        data.lookupOffset = glyphData.lookupOffset;

        instancedata->set(instanceCount, data);
        instanceCount++;

        x += glyphData.xadvance;
    }

    // setup geometry
    auto geometry = GlyphGeometry::create();
    geometry->_glyphInstances = instancedata;

    return geometry;
}

//
// TextGroup
//

TextGroup::TextGroup(Font* font, GraphicsPipeline* pipeline, Allocator* allocator) :
    Inherit(font, pipeline, allocator)
{
}

void TextGroup::addText(const std::string& text, const vec3& position)
{
    _texts.push_back(text);
    _positions.push_back(position);
    //buildTextGraph();
}

void TextGroup::clear()
{
    _texts.clear();
    _positions.clear();
    //buildTextGraph();
}

ref_ptr<GlyphGeometry> TextGroup::createInstancedGlyphs()
{
    // build the instance data
    uint32_t charcount = 0;
    for (auto textstr : _texts)
    {
        charcount += static_cast<uint32_t>(textstr.size());
    }

    // this is a bit wrong at the mo, their might not be charcount instances as we skip new line and space characters
    auto instancedata = GlyphInstanceDataArray::create(charcount);

    uint32_t instanceCount = 0;

    for (uint32_t t = 0; t < _texts.size(); t++)
    {
        float x = 0.0f;
        float y = 0.0f;
        std::string& text = _texts[t];

        for (uint32_t i = 0; i < text.size(); i++)
        {
            uint16_t character = (uint16_t)text.at(i);
            const Font::GlyphData& glyphData = _font->getGlyphMap()[character];

            if (character == '\n')
            {
                y -= _font->getNormalisedLineHeight();
                x = 0.0f;
                continue;
            }
            else if (character == ' ')
            {
                x += glyphData.xadvance;
                continue;
            }

            GlyphInstanceData data;
            data.position = _positions[t];
            data.offset = vec2(x, y);
            data.lookupOffset = glyphData.lookupOffset;

            instancedata->set(instanceCount, data);
            instanceCount++;

            x += glyphData.xadvance;
        }
    }

    auto geometry = GlyphGeometry::create();
    geometry->_glyphInstances = instancedata;

    return geometry;
}

namespace vsg
{
    bool readUnity3dFontMetaFile(const std::string& filePath, Font::GlyphMap& glyphMap, float& fontPixelHeight, float& normalisedLineHeight)
    {
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

            Font::GlyphData glyph;

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
            vec2 uvorigin = vec2(x / scaleWidth, y / scaleHeight);
            vec2 uvsize = vec2(width / scaleWidth, height / scaleHeight);
            glyph.uvrect = vec4(uvorigin.x, uvorigin.y, uvsize.x, uvsize.y);

            // calc normaised size
            glyph.size = vec2(width / fontPixelHeight, height / fontPixelHeight);

            // calc normalise offsets
            glyph.offset = vec2(xoffset / fontPixelHeight, normalisedBaseLine - glyph.size.y - (yoffset / fontPixelHeight));
            glyph.xadvance = xadvance / fontPixelHeight;

            // (the font object will calc this)
            glyph.lookupOffset = 0.0f;

            // add glyph to list
            glyphMap[glyph.character] = glyph;
        }
        return true;
    }
}
