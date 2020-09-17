/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "Text.h"

#include <vsg/io/read.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/commands/Commands.h>
#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/DrawIndexed.h>

#ifdef USE_VSGXCHANGE
#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>
#endif

#include <iostream>

using namespace vsg;

Text::RenderingState::RenderingState(Font* font)
{
    auto textureData = font->atlas;

    // load shaders
    auto vertexShader = read_cast<ShaderStage>("shaders/text.vert", font->options);
    auto fragmentShader = read_cast<ShaderStage>("shaders/text.frag", font->options);

    std::cout<<"vertexShader = "<<vertexShader<<std::endl;
    std::cout<<"fragmentShader = "<<fragmentShader<<std::endl;

    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
    }

#ifdef USE_VSGXCHANGE
    // compile section
    ShaderStages stagesToCompile;
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
    DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    auto descriptorSetLayout = DescriptorSetLayout::create(descriptorBindings);

    PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    VertexInputState::Attributes vertexAttributeDescriptions
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

    auto blending = ColorBlendState::create(ColorBlendState::ColorBlendAttachments{colorBlendAttachment});

    // switch off back face culling
    auto rasterization = RasterizationState::create();
    rasterization->cullMode = VK_CULL_MODE_NONE;

    GraphicsPipelineStates pipelineStates
    {
        VertexInputState::create( vertexBindingsDescriptions, vertexAttributeDescriptions ),
        InputAssemblyState::create(),
        MultisampleState::create(),
        blending,
        rasterization,
        DepthStencilState::create()
    };

    auto pipelineLayout = PipelineLayout::create(DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = GraphicsPipeline::create(pipelineLayout, ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    bindGraphicsPipeline = BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto texture = DescriptorImage::create(Sampler::create(), textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto descriptorSet = DescriptorSet::create(descriptorSetLayout, Descriptors{texture});

    bindDescriptorSet = BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);
}

void Text::setup()
{
    _sharedRenderingState = font->getShared<RenderingState>();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();

    _stategroup = scenegraph;

    scenegraph->add(_sharedRenderingState->bindGraphicsPipeline);
    scenegraph->add(_sharedRenderingState->bindDescriptorSet);

    uint32_t num_quads = 0;
    uint32_t num_rows = 1;
    for(auto& character : text)
    {
        if (character == '\n') ++num_rows;
        else if (character != ' ') ++num_quads;
    }

    // check if we have anything to render
    if (num_quads==0) return;

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
}
