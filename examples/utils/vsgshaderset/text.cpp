/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Array2D.h>
#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/state/ColorBlendState.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/RasterizationState.h>
#include <vsg/text/CpuLayoutTechnique.h>
#include <vsg/text/GpuLayoutTechnique.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/text/Text.h>

vsg::ref_ptr<vsg::ShaderSet> text_ShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Local text_ShaderSet(", options, ")");

    auto shaderSet = vsg::ShaderSet::create();

    // load shaders
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/text.vert", options);
    vertexShader->module->setValue("DebugUtilsName", "VSG built-in text.vert");
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/text.frag", options);
    fragmentShader->module->setValue("DebugUtilsName", "VSG built-in text.frag");

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("text_ShaderSet(...) could not find shaders.");
        return {};
    }

    uint32_t numTextIndices = 256;
    vertexShader->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::uintValue::create(numTextIndices)} // numTextIndices
    };

    shaderSet->stages = vsg::ShaderStages{vertexShader, fragmentShader};

    // used for both CPU and GPU layouts
    shaderSet->addAttributeBinding("inPosition", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addDescriptorBinding("textureAtlas", "", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_ALL, 0, 128);

    // only used when using CPU Layout
    shaderSet->addAttributeBinding("inColor", "CPU_LAYOUT", 1, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
    shaderSet->addAttributeBinding("inOutlineColor", "CPU_LAYOUT", 2, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
    shaderSet->addAttributeBinding("inOutlineWidth", "CPU_LAYOUT", 3, VK_FORMAT_R32_SFLOAT, vsg::floatArray::create(1));
    shaderSet->addAttributeBinding("inTexCoord", "CPU_LAYOUT", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inCenterAndAutoScaleDistance", "BILLBOARD", 5, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    // only used when using GPU Layout
    shaderSet->addDescriptorBinding("glyphMetrics", "GPU_LAYOUT", 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Array2D::create(1, 1));
    shaderSet->addDescriptorBinding("textLayout", "GPU_LAYOUT", 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::TextLayoutValue::create());
    shaderSet->addDescriptorBinding("text", "GPU_LAYOUT", 1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::uivec4Array2D::create(1, 1));

    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    shaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);

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
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    auto colorBlendState = vsg::ColorBlendState::create(vsg::ColorBlendState::ColorBlendAttachments{colorBlendAttachment});
    shaderSet->defaultGraphicsPipelineStates.push_back(colorBlendState);

    shaderSet->setValue("DebugUtilsName", "VSG built-in text ShaderSet");

    return shaderSet;
}
