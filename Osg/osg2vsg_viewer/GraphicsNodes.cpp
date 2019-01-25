#include <vsg/vk/BufferData.h>
#include <vsg/vk/Descriptor.h>
#include <vsg/vk/DescriptorSet.h>
#include <vsg/vk/DescriptorPool.h>
#include <vsg/vk/BindVertexBuffers.h>
#include <vsg/vk/BindIndexBuffer.h>
#include <vsg/vk/Draw.h>

#include <vsg/io/ReaderWriter.h>

#include <vsg/traversals/DispatchTraversal.h>

#include <iostream>

#include "GraphicsNodes.h"

using namespace vsg;


/////////////////////////////////////////////////////////////////////////////////////////
//
// CompileTraversal
//
void CompileTraversal::apply(Group& group)
{
    auto graphics = dynamic_cast<GraphicsNode*>(&group);
    if (graphics)
    {
        apply(*graphics);
    }
    else
    {
        group.traverse(*this);
    }
}

void CompileTraversal::apply(GraphicsNode& graphics)
{
    graphics.compile(context);
    graphics.traverse(*this);
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// GraphicsPipelineGroup
//
GraphicsPipelineGroup::GraphicsPipelineGroup(Allocator* allocator) :
    Inherit(allocator)
{
}

void GraphicsPipelineGroup::accept(DispatchTraversal& dv) const
{
    if (_bindPipeline) _bindPipeline->accept(dv);
    if (_projPushConstant) _projPushConstant->accept(dv);
    if (_viewPushConstant) _viewPushConstant->accept(dv);

    traverse(dv);

    // do we restore pipelines in parental chain?
}

void GraphicsPipelineGroup::traverse(Visitor& visitor)
{
    if (_bindPipeline) _bindPipeline->accept(visitor);
    if (_projPushConstant) _projPushConstant->accept(visitor);
    if (_viewPushConstant) _viewPushConstant->accept(visitor);

    Inherit::traverse(visitor);
}

void GraphicsPipelineGroup::traverse(ConstVisitor& visitor) const
{
    if (_bindPipeline) _bindPipeline->accept(visitor);
    if (_projPushConstant) _projPushConstant->accept(visitor);
    if (_viewPushConstant) _viewPushConstant->accept(visitor);

    Inherit::traverse(visitor);
}

void GraphicsPipelineGroup::compile(Context& context)
{
    //
    // set up descriptor layout and descriptor set and pipeline layout for uniforms
    //
    context.descriptorPool = DescriptorPool::create(context.device, maxSets, descriptorPoolSizes);
    context.descriptorSetLayout = DescriptorSetLayout::create(context.device, descriptorSetLayoutBindings);
    context.pipelineLayout = PipelineLayout::create(context.device, {context.descriptorSetLayout}, pushConstantRanges);


    ShaderModules shaderModules;
    shaderModules.reserve(shaders.size());
    for(auto& shader : shaders)
    {
        shaderModules.emplace_back(ShaderModule::create(context.device, shader));
    }

    auto shaderStages = ShaderStages::create(shaderModules);

    GraphicsPipelineStates full_pipelineStates = pipelineStates;
    full_pipelineStates.emplace_back(context.viewport);
    full_pipelineStates.emplace_back(shaderStages);
    full_pipelineStates.emplace_back(VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions));


    ref_ptr<GraphicsPipeline> pipeline = GraphicsPipeline::create(context.device, context.renderPass, context.pipelineLayout, full_pipelineStates);

    _bindPipeline = BindPipeline::create(pipeline);

    if (context.projMatrix) _projPushConstant = PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 0, context.projMatrix);
    if (context.viewMatrix) _viewPushConstant = PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 64, context.viewMatrix);
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// Texture
//
Texture::Texture(Allocator* allocator) :
    Inherit(allocator)
{
}

void Texture::traverse(Visitor& visitor)
{
    if (_bindDescriptorSets) _bindDescriptorSets->accept(visitor);
    Inherit::traverse(visitor);
}

void Texture::traverse(ConstVisitor& visitor) const
{
    if (_bindDescriptorSets) _bindDescriptorSets->accept(visitor);
    Inherit::traverse(visitor);
}


void Texture::accept(DispatchTraversal& dv) const
{
    if (_bindDescriptorSets) _bindDescriptorSets->accept(dv);
    traverse(dv);
}


void Texture::compile(Context& context)
{
    vsg::ImageData imageData = vsg::transferImageData(context.device, context.commandPool, context.graphicsQueue, _textureData);
    if (!imageData.valid())
    {
        std::cout<<"Texture not created"<<std::endl;
        return;
    }

    // set up DescriptorSet
    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(context.device, context.descriptorPool, context.descriptorSetLayout,
    {
        vsg::DescriptorImage::create(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{imageData})
    });

    // setup binding of descriptors
    _bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipelineLayout, vsg::DescriptorSets{descriptorSet}); // device dependent
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// MatrixTransform
//
MatrixTransform::MatrixTransform(Allocator* allocator) :
    Inherit(allocator)
{
    _matrix = new mat4Value;
}

void MatrixTransform::accept(DispatchTraversal& dv) const
{
    if (_pushConstant) _pushConstant->accept(dv);

    traverse(dv);

    // do we restore transforms in parental chain?
}

void MatrixTransform::compile(Context& context)
{
    _pushConstant = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 128, _matrix);
}

//
//  Geometry node
//       vertex arrays
//       index arrays
//       draw + draw DrawIndexed
//       push constants for per geometry colours etc.
//
//       Maps to a Group containing StateGroup + Binds + DrawIndex/Draw etc + Push constants
//
/////////////////////////////////////////////////////////////////////////////////////////
//
// Geometry
//
Geometry::Geometry(Allocator* allocator) :
    Inherit(allocator)
{
}

void Geometry::accept(DispatchTraversal& dv) const
{
    std::cout<<"Geometry::accept(DisptatchTraversal& dv) enter"<<std::endl;
    std::cout<<"    _arrays.size() = "<<_arrays.size() <<std::endl;
    std::cout<<"    _indices.get() = "<<_indices.get() <<std::endl;
    std::cout<<"    _commands.size() = "<<_commands.size() <<std::endl;
    if (_renderImplementation) _renderImplementation->accept(dv);
    std::cout<<"Geometry::accept(DisptatchTraversal& dv) leave"<<std::endl;
}

void Geometry::compile(Context& context)
{
    auto vertexBufferData = vsg::createBufferAndTransferData(context.device, context.commandPool, context.graphicsQueue, _arrays, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    auto indexBufferData = vsg::createBufferAndTransferData(context.device, context.commandPool, context.graphicsQueue, {_indices}, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

    _renderImplementation = new vsg::Group;

    // set up vertex buffer binding
    vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);  // device dependent
    _renderImplementation->addChild(bindVertexBuffers); // device dependent

    // set up index buffer binding
    vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(indexBufferData.front(), VK_INDEX_TYPE_UINT16); // device dependent
    _renderImplementation->addChild(bindIndexBuffer); // device dependent

    // add the commands in the the _renderImplementation group.
    for(auto& command : _commands) _renderImplementation->addChild(command);
}
