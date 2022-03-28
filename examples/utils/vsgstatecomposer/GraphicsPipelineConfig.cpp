#include "GraphicsPipelineConfig.h"

using namespace vsg;

GraphicsPipelineConfig::GraphicsPipelineConfig(ref_ptr<ShaderSet> in_shaderSet) :
    shaderSet(in_shaderSet)
{
    vertexInputState = VertexInputState::create();
    inputAssemblyState = vsg::InputAssemblyState::create();
    rasterizationState = vsg::RasterizationState::create();
    colorBlendState = vsg::ColorBlendState::create();
    multisampleState = vsg::MultisampleState::create();
    depthStencilState = vsg::DepthStencilState::create();

    shaderHints = vsg::ShaderCompileSettings::create();
}

bool GraphicsPipelineConfig::assignArray(DataList& arrays, const std::string& name, VkVertexInputRate vertexInputRate, ref_ptr<Data> array)
{
    auto& attributeBinding = shaderSet->getAttributeBinding(name);
    if (attributeBinding)
    {
        if (!attributeBinding.define.empty()) shaderHints->defines.push_back(attributeBinding.define);

        uint32_t bindingIndex = baseAttributeBinding + static_cast<uint32_t>(arrays.size());
        vertexInputState->vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{attributeBinding.location, bindingIndex, attributeBinding.format, 0});
        vertexInputState->vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{bindingIndex, array->getLayout().stride, vertexInputRate});
        arrays.push_back(array);
        return true;
    }
    return false;
}

const UniformBinding& GraphicsPipelineConfig::getUniformBinding(const std::string& name)
{
    auto& uniformBinding = shaderSet->getUniformBinding(name);
    if (uniformBinding)
    {
        if (!uniformBinding.define.empty()) shaderHints->defines.push_back(uniformBinding.define);

        descriptorBindings.push_back(VkDescriptorSetLayoutBinding{uniformBinding.binding, uniformBinding.descriptorType, uniformBinding.descriptorCount, uniformBinding.stageFlags, nullptr});
    }
    return uniformBinding;
}

void GraphicsPipelineConfig::init()
{
    descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges;
    for(auto& pcb : shaderSet->pushConstantRanges)
    {
        if (pcb.define.empty()) pushConstantRanges.push_back(pcb.range);
    }

    layout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);

    GraphicsPipelineStates pipelineStates{vertexInputState, inputAssemblyState, rasterizationState, colorBlendState, multisampleState, depthStencilState};

    graphicsPipeline = GraphicsPipeline::create(layout, shaderSet->getShaderStages(shaderHints), pipelineStates, subpass);
    bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);
}


int GraphicsPipelineConfig::compare(const Object& rhs) const
{
    return Object::compare(rhs);
}
