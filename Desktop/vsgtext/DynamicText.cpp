/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Array2D.h>
#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/commands/Commands.h>
#include <vsg/commands/DrawIndexed.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/state/DescriptorImage.h>

#include "DynamicText.h"

#include <iostream>

//#include "shaders/text_frag.cpp"
//#include "shaders/text_vert.cpp"

using namespace vsg;

void DynamicText::read(Input& input)
{
    Node::read(input);

    input.readObject("font", font);
    input.readObject("layout", layout);
    input.readObject("text", text);

    setup();
}

void DynamicText::write(Output& output) const
{
    Node::write(output);

    output.writeObject("font", font);
    output.writeObject("layout", layout);
    output.writeObject("text", text);
}

DynamicText::FontState::FontState(Font* font)
{
    std::cout<<"DynamicText::FontState::FontState(Font* font)"<<std::endl;

    {
        auto sampler = Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler->borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        sampler->anisotropyEnable = VK_TRUE;
        sampler->maxAnisotropy = 16.0f;
        sampler->magFilter = VK_FILTER_LINEAR;
        sampler->minFilter = VK_FILTER_LINEAR;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler->maxLod = 12.0;

        textureAtlas = DescriptorImage::create(sampler, font->atlas, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    {
        auto sampler = Sampler::create();
        sampler->magFilter = VK_FILTER_NEAREST;
        sampler->minFilter = VK_FILTER_NEAREST;
        sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler->unnormalizedCoordinates = VK_TRUE;

        auto glyphMetricsProxy = floatArray2D::create(font->glyphMetrics, 0, sizeof(GlyphMetrics), sizeof(GlyphMetrics)/sizeof(float), font->glyphMetrics->valueCount(), Data::Layout{VK_FORMAT_R32_SFLOAT});
        glyphMetrics = DescriptorImage::create(sampler, glyphMetricsProxy, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        vsg::write(glyphMetricsProxy, "glyphMetrics.vsgt");
    }
}

DynamicText::RenderingState::RenderingState(Font* font)
{
    std::cout<<"DynamicText::RenderingState::RenderingState(Font* font, ...)"<<std::endl;

    // load shaders
    auto vertexShader = read_cast<ShaderStage>("shaders/dynamic_text.vert", font->options);
    //if (!vertexShader) vertexShader = text_vert(); // fallback to shaders/text_vert.cppp

    auto fragmentShader = read_cast<ShaderStage>("shaders/text.frag", font->options);
    //if (!fragmentShader) fragmentShader = text_frag(); // fallback to shaders/text_frag.cppp

    // compile section
    ShaderStages stagesToCompile;
    if (vertexShader && vertexShader->module && vertexShader->module->code.empty()) stagesToCompile.emplace_back(vertexShader);
    if (fragmentShader && fragmentShader->module && fragmentShader->module->code.empty()) stagesToCompile.emplace_back(fragmentShader);

    uint32_t numGlyphMetrics = static_cast<uint32_t>(font->glyphMetrics->valueCount());
    uint32_t numCharacters = static_cast<uint32_t>(font->charmap->valueCount());

    vertexShader->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::uintValue::create(numGlyphMetrics)} // numGlyphMetrics
    };

    // Glyph
    DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // texture atlas
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } // glyph matrices
    };

    auto descriptorSetLayout = DescriptorSetLayout::create(descriptorBindings);

    // set up graphics pipeline
    DescriptorSetLayoutBindings textArrayDescriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr }, // Layout uniform
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } // Text uniform
    };

    textArrayDescriptorSetLayout = DescriptorSetLayout::create(textArrayDescriptorBindings);

    PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX},                                                       // vertex data
    };

    VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},    // vertex data
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

    auto blending = ColorBlendState::create(ColorBlendState::ColorBlendAttachments{colorBlendAttachment});

    // switch off back face culling
    auto rasterization = RasterizationState::create();
    rasterization->cullMode = VK_CULL_MODE_NONE;

    GraphicsPipelineStates pipelineStates{
        VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        InputAssemblyState::create(),
        MultisampleState::create(),
        blending,
        rasterization,
        DepthStencilState::create()};

    pipelineLayout = PipelineLayout::create(DescriptorSetLayouts{descriptorSetLayout, textArrayDescriptorSetLayout}, pushConstantRanges);

    auto graphicsPipeline = GraphicsPipeline::create(pipelineLayout, ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    bindGraphicsPipeline = BindGraphicsPipeline::create(graphicsPipeline);


    // create texture image and associated DescriptorSets and binding
    auto fontState = font->getShared<FontState>();
    auto descriptorSet = DescriptorSet::create(descriptorSetLayout, Descriptors{fontState->textureAtlas, fontState->glyphMetrics});

    bindDescriptorSet = BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);
}

template<typename T> void assignValue(T& dest, const T& src, bool& updated)
{
    if (dest==src) return;
    dest = src;
    updated = true;
}

void DynamicText::setup(uint32_t minimumAllocation)
{
    if (!layout) layout = LeftAlignment::create();

    bool textLayoutUpdated = false;
    bool textArrayUpdated = false;

    struct ConvertString : public ConstVisitor
    {
        Font& font;
        ref_ptr<uintArray>& textArray;
        bool& updated;
        size_t minimumSize = 0;
        size_t size = 0;

        ConvertString(Font& in_font, ref_ptr<uintArray>& in_textArray, bool& in_updated, size_t in_minimumSize) :
            font(in_font),
            textArray(in_textArray),
            updated(in_updated),
            minimumSize(in_minimumSize) {}

        void allocate(size_t allocationSize)
        {
            size = allocationSize;

            if (allocationSize < minimumSize) allocationSize = minimumSize;
            if (!textArray || allocationSize > textArray->valueCount())
            {
                updated = true;
                textArray = uintArray::create(allocationSize);
            }
        }

        void apply(const stringValue& text) override
        {
            allocate(text.value().size());

            auto itr = textArray->begin();
            for (auto& c : text.value())
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(uint8_t(c)), updated);
            }
        }
        void apply(const ubyteArray& text) override
        {
            allocate(text.valueCount());

            auto itr = textArray->begin();
            for (auto& c : text)
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(c), updated);
            }
        }
        void apply(const ushortArray& text) override
        {
            allocate(text.valueCount());

            auto itr = textArray->begin();
            for (auto& c : text)
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(c), updated);
            }
        }
        void apply(const uintArray& text) override
        {
            allocate(text.valueCount());

            auto itr = textArray->begin();
            for (auto& c : text)
            {
                assignValue(*(itr++), font.glyphIndexForCharcode(c), updated);
            }
        }
    };

#if 1
    TextQuads quads;
    layout->layout(text, *font, quads);
#endif

    if (!renderingBackend)
    {
#if 1
        if (quads.empty()) return;
#endif
        renderingBackend = RenderingBackend::create();
    }

    ConvertString convert(*font, renderingBackend->textArray, textArrayUpdated, minimumAllocation);
    text->accept(convert);

    auto textArray = convert.textArray;
    size_t num_quads = convert.size;

    // TODO need to reallocate DescriptorBuffer if textArray changes size?
    auto& textDescriptor = renderingBackend->textDescriptor;
    if (!textDescriptor) textDescriptor = DescriptorBuffer::create(textArray, 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);


    // set up the layout data in a form digestable by the GPU.
    auto& layoutValue = renderingBackend->layoutValue;
    if (!layoutValue) layoutValue = TextLayoutValue::create();

    auto& layoutStruct = layoutValue->value();
    if (auto leftAlignment = layout.cast<LeftAlignment>(); leftAlignment)
    {
        assignValue(layoutStruct.position, leftAlignment->position, textLayoutUpdated);
        assignValue(layoutStruct.horizontal, leftAlignment->horizontal, textLayoutUpdated);
        assignValue(layoutStruct.vertical, leftAlignment->vertical, textLayoutUpdated);
        assignValue(layoutStruct.color, leftAlignment->color, textLayoutUpdated);
        assignValue(layoutStruct.outlineColor, leftAlignment->outlineColor, textLayoutUpdated);
        assignValue(layoutStruct.outlineWidth, leftAlignment->outlineWidth, textLayoutUpdated);
    }

    auto& layoutDescriptor = renderingBackend->layoutDescriptor;
    if (!layoutDescriptor) layoutDescriptor = DescriptorBuffer::create(layoutValue, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    auto& vertices = renderingBackend->vertices;
    auto& drawIndexed = renderingBackend->drawIndexed;

    size_t num_vertices = 4;

    if (!vertices || num_vertices > vertices->size())
    {
        vertices = vec3Array::create(num_vertices);

        float leadingEdgeGradient = 0.1f;

        vertices->set(0, vec3(0.0f, 0.0f, leadingEdgeGradient));
        vertices->set(1, vec3(1.0f, 0.0f, 0.0f));
        vertices->set(2, vec3(1.0f, 1.0f, leadingEdgeGradient));
        vertices->set(3, vec3(0.0f, 1.0f, 2.0*leadingEdgeGradient));
    }

    auto& indices = renderingBackend->indices;
    if (!indices || 6 > indices->valueCount())
    {
        auto us_indices = ushortArray::create(6);
        indices = us_indices;

        us_indices->set(0, 0);
        us_indices->set(1, 1);
        us_indices->set(2, 2);
        us_indices->set(3, 2);
        us_indices->set(4, 3);
        us_indices->set(5, 0);
    }

    if (!drawIndexed)
        drawIndexed = DrawIndexed::create(6, num_quads, 0, 0, 0);
    else
        drawIndexed->instanceCount = num_quads;

    // std::cout<<"drawIndexed->instanceCount = "<<drawIndexed->instanceCount<<std::endl;

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto& scenegraph = renderingBackend->stategroup;
    auto& bindVertexBuffers = renderingBackend->bindVertexBuffers;
    auto& bindIndexBuffer = renderingBackend->bindIndexBuffer;
    if (!scenegraph)
    {
        scenegraph = StateGroup::create();

        // set up state related objects if they haven't lready been assigned
        auto& sharedRenderingState = renderingBackend->sharedRenderingState;
        if (!sharedRenderingState) renderingBackend->sharedRenderingState = font->getShared<RenderingState>();

        if (sharedRenderingState->bindGraphicsPipeline) scenegraph->add(sharedRenderingState->bindGraphicsPipeline);
        if (sharedRenderingState->bindDescriptorSet) scenegraph->add(sharedRenderingState->bindDescriptorSet);

        // set up graphics pipeline
        auto textDescriptorSet = DescriptorSet::create(sharedRenderingState->textArrayDescriptorSetLayout, Descriptors{layoutDescriptor, textDescriptor});
        renderingBackend->bindTextDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, sharedRenderingState->pipelineLayout, 1, textDescriptorSet);
        scenegraph->add(renderingBackend->bindTextDescriptorSet);

        bindVertexBuffers = BindVertexBuffers::create(0, DataList{vertices});
        bindIndexBuffer = BindIndexBuffer::create(indices);

        // setup geometry
        auto drawCommands = Commands::create();
        drawCommands->addChild(bindVertexBuffers);
        drawCommands->addChild(bindIndexBuffer);
        drawCommands->addChild(drawIndexed);

        scenegraph->addChild(drawCommands);

        //vsg::write(scenegraph, "scenegraph.vsgt");
    }
    else
    {
        if (textArrayUpdated)
        {
            textDescriptor->copyDataListToBuffers();
        }
        if (textLayoutUpdated)
        {
            layoutDescriptor->copyDataListToBuffers();
        }
    }
}
