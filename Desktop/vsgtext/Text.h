#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */


#include <vsg/nodes/Node.h>

namespace vsg
{

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

}
