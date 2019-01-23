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

GraphicsPipelineConfig::GraphicsPipelineConfig(Allocator* allocator) :
    Inherit(allocator)
{
}

void GraphicsPipelineConfig::init()
{
    maxSets = 0;
    descriptorPoolSizes = DescriptorPoolSizes
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // texture
    };

    descriptorSetLayoutBindings = vsg::DescriptorSetLayoutBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    };

    pushConstantRanges = vsg::PushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
    };

    vertexBindingsDescriptions = vsg::VertexInputState::Bindings
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vertexAttributeDescriptions = vsg::VertexInputState::Attributes
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    pipelineStates = GraphicsPipelineStates
    {
        vsg::InputAssemblyState::create(), // device independent
        vsg::RasterizationState::create(),// device independent
        vsg::MultisampleState::create(),// device independent
        vsg::ColorBlendState::create(),// device independent
        vsg::DepthStencilState::create()// device independent
    };
}

GraphicsPipeline* GraphicsPipelineConfig::createPipeline(Device* device, RenderPass* renderPass, ViewportState* viewport)
{
    //
    // set up descriptor layout and descriptor set and pipeline layout for uniforms
    //
    vsg::ref_ptr<vsg::DescriptorPool> descriptorPool = vsg::DescriptorPool::create(device, maxSets, descriptorPoolSizes);

    vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout = vsg::DescriptorSetLayout::create(device, descriptorSetLayoutBindings);

    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout = vsg::PipelineLayout::create(device, {descriptorSetLayout}, pushConstantRanges);


    vsg::ShaderModules shaderModules;
    shaderModules.reserve(shaders.size());
    for(auto& shader : shaders)
    {
        shaderModules.emplace_back(vsg::ShaderModule::create(device, shader));
    }

    auto shaderStages = vsg::ShaderStages::create(shaderModules);

    GraphicsPipelineStates full_pipelineStates = pipelineStates;
    pipelineStates.emplace_back(viewport);
    pipelineStates.emplace_back(shaderStages);
    pipelineStates.emplace_back(vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions));


    vsg::ref_ptr<vsg::GraphicsPipeline> pipeline = vsg::GraphicsPipeline::create(device, renderPass, pipelineLayout, full_pipelineStates);
    return pipeline;
}



/////////////////////////////////////////////////////////////////////////////////////////
//
// MatrixTransform
//
MatrixTransform::MatrixTransform(Allocator* allocator) :
    Inherit(allocator)
{
}

void MatrixTransform::accept(DispatchTraversal& dv) const
{
}

void MatrixTransform::compile(Context& context)
{
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
    if (_stateGroup) _stateGroup->accept(dv);
    std::cout<<"Geometry::accept(DisptatchTraversal& dv) leave"<<std::endl;
}

void Geometry::compile(Context& context)
{
    auto vertexBufferData = vsg::createBufferAndTransferData(context.device, context.commandPool, context.graphicsQueue, _arrays, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    auto indexBufferData = vsg::createBufferAndTransferData(context.device, context.commandPool, context.graphicsQueue, {_indices}, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

    _stateGroup = new vsg::StateGroup;

    // set up vertex buffer binding
    vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);  // device dependent
    _stateGroup->add(bindVertexBuffers); // device dependent

    // set up index buffer binding
    vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(indexBufferData.front(), VK_INDEX_TYPE_UINT16); // device dependent
    _stateGroup->add(bindIndexBuffer); // device dependent

    // set up drawing of the triangles
    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent
    _stateGroup->addChild(drawIndexed); // device independent
}

///////////////////////
//
//
//  To select required GraphicsPipeline we have select from inputs:
//        device, renderPass,
//        pipelineLayout // {descriptorSetLayout}, pushConstantRanges)
//        shaderStages,  // device dependent {vertexShader,fragmentShader}
//        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),// device independent
//        vsg::InputAssemblyState::create(), // device independent
//        viewport, // window independent
//        vsg::RasterizationState::create(),// device independent
//        vsg::MultisampleState::create(),// device independent
//        vsg::ColorBlendState::create(),// device independent
//        vsg::DepthStencilState::create()// device independent
//
//
//  State (populated by StateGroup)
//        StateStack<BindPipeline> pipelineStack;
//        StateStack<BindDescriptorSets> descriptorStack;  (imageData -> Array2D image )
//        StateStack<BindVertexBuffers> vertexBuffersStack; (BufferData -> Array
//        StateStack<BindIndexBuffer> indexBufferStack; (BufferData -> Array> )
//        PushConstantsMap pushConstantsMap; (projection, view and model matrices)
//
//  StateGroup
//       settings for Program
//       settings for Textures
//       settings for Uniforms
//       Texture -> [VkSamplerCreateInfo->Sampler, VkImageLayout, Data] -> ImageData
//
StateSet::StateSet(Allocator* allocator) :
    Inherit(allocator)
{
}

void StateSet::accept(DispatchTraversal& dv) const
{
    std::cout<<"StateSet::accept(DisptatchTraversal& dv) enter"<<std::endl;
    dv.apply(*this);
    std::cout<<"StateSet::accept(DisptatchTraversal& dv) leave"<<std::endl;
}


void StateSet::compile(Context& context)
{
    vsg::ImageData imageData;

    if (textureData)
    {
        imageData = vsg::transferImageData(context.device, context.commandPool, context.graphicsQueue, textureData);
        if (!imageData.valid())
        {
            std::cout<<"Texture not created"<<std::endl;
        }
    }

    //
    // set up what we want to render in a command graph
    // create command graph to contain all the Vulkan calls for specifically rendering the model
    //
    auto commandGraph = vsg::StateGroup::create();

    // variables assigned within rendering setup, but needed by model setup
    vsg::ref_ptr<vsg::DescriptorPool> descriptorPool;
    vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout;
    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout;

    // create rendering setup
    {
        //
        // set up descriptor layout and descriptor set and pipeline layout for uniforms
        //
        descriptorPool = vsg::DescriptorPool::create(context.device, 1,
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // texture
        });

        descriptorSetLayout = vsg::DescriptorSetLayout::create(context.device,
        {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // texture
        });

        vsg::PushConstantRanges pushConstantRanges
        {
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
        };

        pipelineLayout = vsg::PipelineLayout::create(context.device, {descriptorSetLayout}, pushConstantRanges);


        // set up graphics pipeline
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

        vsg::ref_ptr<vsg::ShaderStages> shaderStages = vsg::ShaderStages::create(vsg::ShaderModules
        {
            vsg::ShaderModule::create(context.device, vertexShader),
            vsg::ShaderModule::create(context.device, fragmentShader)
        });

        vsg::ref_ptr<vsg::GraphicsPipeline> pipeline = vsg::GraphicsPipeline::create(context.device, context.renderPass, pipelineLayout, // device dependent
        {
            shaderStages,  // device dependent
            vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),// device independent
            vsg::InputAssemblyState::create(), // device independent
            context.viewport, // window independent
            vsg::RasterizationState::create(),// device independent
            vsg::MultisampleState::create(),// device independent
            vsg::ColorBlendState::create(),// device independent
            vsg::DepthStencilState::create()// device independent
        });

        vsg::ref_ptr<vsg::BindPipeline> bindPipeline = vsg::BindPipeline::create(pipeline);

        // set up the state configuration
        commandGraph->add(bindPipeline);  // device dependent
    }

}

/////////////////////////////////////////////////////////////////////////////////////////
//
// CompileTraversal
//
void CompileTraversal::apply(Group& group)
{
    const std::type_info& type_group = typeid(group);
    if (type_group==typeid(vsg::Geometry))
    {
        apply(static_cast<vsg::Geometry&>(group));
    }
    else if (type_group==typeid(vsg::MatrixTransform))
    {
        apply(static_cast<vsg::MatrixTransform&>(group));
    }
    else if (type_group==typeid(vsg::StateSet))
    {
        apply(static_cast<vsg::StateSet&>(group));
    }
    else
    {
        group.traverse(*this);
    }
}

void CompileTraversal::apply(Geometry& geometry)
{
    geometry.compile(context);
}

void CompileTraversal::apply(MatrixTransform& transform)
{
    transform.compile(context);
}

void CompileTraversal::apply(StateSet& stateset)
{
    stateset.compile(context);
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

    vsg::ref_ptr<vsg::StateSet> shaderStateSet = vsg::StateSet::create();

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

    // to convert into BindDescriptor/DescriptorPool
    // VkDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT

    // set up vertex and index arrays
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
    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent

    // setup gepmetry
    geometry->_arrays = vsg::DataList{vertices, colors, texcoords};
    geometry->_indices = indices;
    geometry->_commands = vsg::Geometry::Commands{drawIndexed};


    auto textureStateSet = vsg::StateSet::create();
    textureStateSet->addChild(geometry);
    textureStateSet->textureData = textureData;

    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT, 128
    transform->addChild(textureStateSet);

    shaderStateSet->addChild(transform);

    return shaderStateSet;
}
