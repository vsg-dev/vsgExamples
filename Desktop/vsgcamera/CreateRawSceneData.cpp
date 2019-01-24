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

#include "CreateRawSceneData.h"





namespace vsg
{

GraphicsPipelineGroup::GraphicsPipelineGroup(Allocator* allocator) :
    Inherit(allocator)
{
}

void GraphicsPipelineGroup::init()
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
    pipelineStates.emplace_back(context.viewport);
    pipelineStates.emplace_back(shaderStages);
    pipelineStates.emplace_back(VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions));


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

    // set up drawing of the triangles
    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent
    _renderImplementation->addChild(drawIndexed); // device independent
}

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


} // namespace vsg

/////////////////////////////////////////////////////////////////////////////////////////
//
// createRawSceneData
//
vsg::ref_ptr<vsg::Node> createRawSceneData(vsg::Paths& searchPaths)
{
    //
    // load shaders
    //
    vsg::ref_ptr<vsg::Shader> vertexShader = vsg::Shader::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::Shader> fragmentShader = vsg::Shader::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return vsg::ref_ptr<vsg::Node>();
    }

    std::string textureFile("textures/lz.vsgb");

    //
    // set up texture image
    //
    vsg::vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile(textureFile, searchPaths));
    if (!textureData)
    {
        std::cout<<"Could not read texture file : "<<textureFile<<std::endl;
        return vsg::ref_ptr<vsg::Node>();
    }

    //
    // set up graphics pipeline
    //
    vsg::ref_ptr<vsg::GraphicsPipelineGroup> gp = vsg::GraphicsPipelineGroup::create();

    gp->shaders = vsg::GraphicsPipelineGroup::Shaders{vertexShader, fragmentShader};
    gp->maxSets = 0;
    gp->descriptorPoolSizes = vsg::DescriptorPoolSizes
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // texture
    };

    gp->descriptorSetLayoutBindings = vsg::DescriptorSetLayoutBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    gp->pushConstantRanges = vsg::PushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
    };

    gp->vertexBindingsDescriptions = vsg::VertexInputState::Bindings
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    gp->vertexAttributeDescriptions = vsg::VertexInputState::Attributes
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    gp->pipelineStates = vsg::GraphicsPipelineStates
    {
        vsg::InputAssemblyState::create(), // device independent
        vsg::RasterizationState::create(),// device independent
        vsg::MultisampleState::create(),// device independent
        vsg::ColorBlendState::create(),// device independent
        vsg::DepthStencilState::create()// device independent
    };


    //
    // set up model transformation node
    //
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT, 128

    // add transform to graphics pipeline group
    gp->addChild(transform);


    //
    // create texture node
    //
    vsg::ref_ptr<vsg::Texture> texture = vsg::Texture::create();
    texture->_textureData = textureData;

    // add texture node to transform node
    transform->addChild(texture);


    //
    // set up vertex and index arrays
    //
    vsg::ref_ptr<vsg::vec3Array> vertices(new vsg::vec3Array
    {
        {-0.5f, -0.5f, 0.0f},
        {0.5f,  -0.5f, 0.05f},
        {0.5f , 0.5f, 0.0f},
        {-0.5f, 0.5f, 0.0f},
        {-0.5f, -0.5f, -0.5f},
        {0.5f,  -0.5f, -0.5f},
        {0.5f , 0.5f, -0.5},
        {-0.5f, 0.5f, -0.5}
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    vsg::ref_ptr<vsg::vec3Array> colors(new vsg::vec3Array
    {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    vsg::ref_ptr<vsg::vec2Array> texcoords(new vsg::vec2Array
    {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray
    {
        0, 1, 2,
        2, 3, 0,
        4, 5, 6,
        6, 7, 4
    }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto geometry = vsg::Geometry::create();

    // setup geometry
    geometry->_arrays = vsg::DataList{vertices, colors, texcoords};
    geometry->_indices = indices;

    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent
    geometry->_commands = vsg::Geometry::Commands{drawIndexed};

    // add geometry to texture group
    texture->addChild(geometry);

    return gp;
}
