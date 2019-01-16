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

#include "CreateSceneData.h"


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
//  DrawIndexed - direct node attached to command graph


//
//  Geometry node
//       vertex arrays
//       index arrays
//       draw + draw DrawIndexed
//       push constants for per geometry colours etc.
//
//       Maps to a Group containing StateGroup + Binds + DrawIndex/Draw etc + Push constants
//
//  Transform node
//       has a matrix/compute modelview matrix, push on State stack (have multiple matrix stacks?)
//
//
//  StateGroup
//       settings for Program
//       settings for Textures
//       settings for Uniforms

namespace vsg
{

    class MatrixTransform : public Inherit<Group, MatrixTransform>
    {
    public:
        MatrixTransform(Allocator* allocator = nullptr)
        {
        }

        void accept(DispatchTraversal& dv) const override
        {
            std::cout<<"MatrixTransform::accept(DisptatchTraversal& dv) enter"<<std::endl;
            dv.apply(*this);
            std::cout<<"MatrixTransform::accept(DisptatchTraversal& dv) leave"<<std::endl;
        }

        dmat4 _matrix;
    };

    class Geometry : public Inherit<Group, Geometry>
    {
    public:
        Geometry(Allocator* allocator = nullptr)
        {
        }

        void accept(DispatchTraversal& dv) const override
        {
            std::cout<<"Geometry::accept(DisptatchTraversal& dv) enter"<<std::endl;
            std::cout<<"    _arrays.size() = "<<_arrays.size() <<std::endl;
            std::cout<<"    _indices.get() = "<<_indices.get() <<std::endl;
            std::cout<<"    _commands.size() = "<<_commands.size() <<std::endl;
            dv.apply(*this);
            std::cout<<"Geometry::accept(DisptatchTraversal& dv) leave"<<std::endl;
        }

        using Arrays = std::vector<ref_ptr<Data>>;
        using Commands = std::vector<ref_ptr<Command>>;

        Arrays _arrays;
        ref_ptr<Data> _indices;
        Commands _commands;
    };


}

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
    geometry->_arrays = vsg::Geometry::Arrays{vertices, colors, texcoords};
    geometry->_indices = indices;
    geometry->_commands = vsg::Geometry::Commands{drawIndexed};

    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT, 128
    transform->addChild(geometry);

    return transform;
}


vsg::ref_ptr<vsg::Node> createSceneData(vsg::ref_ptr<vsg::Device> device, vsg::ref_ptr<vsg::CommandPool> commandPool, vsg::ref_ptr<vsg::RenderPass> renderPass, VkQueue graphicsQueue, // viewer/window
                                        vsg::ref_ptr<vsg::mat4Value> projMatrix, vsg::ref_ptr<vsg::mat4Value> viewMatrix, vsg::ref_ptr<vsg::ViewportState> viewport, // camera
                                        vsg::Paths& searchPaths) // scene graph
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

    vsg::ImageData imageData = vsg::transferImageData(device, commandPool, graphicsQueue, textureData);
    if (!imageData.valid())
    {
        std::cout<<"Texture not created"<<std::endl;
        return vsg::ref_ptr<vsg::Node>();
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
        descriptorPool = vsg::DescriptorPool::create(device, 1,
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // texture
        });

        descriptorSetLayout = vsg::DescriptorSetLayout::create(device,
        {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // texture
        });

        vsg::PushConstantRanges pushConstantRanges
        {
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
        };

        pipelineLayout = vsg::PipelineLayout::create(device, {descriptorSetLayout}, pushConstantRanges);


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
            vsg::ShaderModule::create(device, vertexShader),
            vsg::ShaderModule::create(device, fragmentShader)
        });

        vsg::ref_ptr<vsg::GraphicsPipeline> pipeline = vsg::GraphicsPipeline::create(device, renderPass, pipelineLayout, // device dependent
        {
            shaderStages,  // device dependent
            vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),// device independent
            vsg::InputAssemblyState::create(), // device independent
            viewport, // window independent
            vsg::RasterizationState::create(),// device independent
            vsg::MultisampleState::create(),// device independent
            vsg::ColorBlendState::create(),// device independent
            vsg::DepthStencilState::create()// device independent
        });

        vsg::ref_ptr<vsg::BindPipeline> bindPipeline = vsg::BindPipeline::create(pipeline);

        // set up the state configuration
        commandGraph->add(bindPipeline);  // device dependent
    }

    // setup camera values
    {
        // camera projection matrix
        vsg::ref_ptr<vsg::PushConstants> pushConstant_proj = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 0, projMatrix);
        commandGraph->add(pushConstant_proj);

        // camera view matrix
        vsg::ref_ptr<vsg::PushConstants> pushConstant_view = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 64, viewMatrix);
        commandGraph->add(pushConstant_view);
    }

    // create model
    {
        // add subgraph that represents the model to render
        vsg::ref_ptr<vsg::StateGroup> model = vsg::StateGroup::create();


        // set up DescriptorSet
        vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(device, descriptorPool, descriptorSetLayout,
        {
            vsg::DescriptorImage::create(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{imageData})
        });

        // setup binding of descriptors
        vsg::ref_ptr<vsg::BindDescriptorSets> bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, vsg::DescriptorSets{descriptorSet}); // device dependent
        model->add(bindDescriptorSets);  // device dependent


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
        });

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
        });

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
        });

        vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray
        {
            0, 1, 2,
            2, 3, 0,
            4, 5, 6,
            6, 7, 4
        });


        auto vertexBufferData = vsg::createBufferAndTransferData(device, commandPool, graphicsQueue, vsg::DataList{vertices, colors, texcoords}, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
        auto indexBufferData = vsg::createBufferAndTransferData(device, commandPool, graphicsQueue, {indices}, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);


        // set up vertex buffer binding
        vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);  // device dependent
        model->add(bindVertexBuffers); // device dependent

        // set up index buffer binding
        vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(indexBufferData.front(), VK_INDEX_TYPE_UINT16); // device dependent
        model->add(bindIndexBuffer); // device dependent

        // model transform matrix
        vsg::ref_ptr<vsg::mat4Value> modelMatrix(new vsg::mat4Value);
        vsg::ref_ptr<vsg::PushConstants> pushConstant_model = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 128, modelMatrix); // device independent
        model->add(pushConstant_model);

        // set up drawing of the triangles
        vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent
        model->addChild(drawIndexed); // device independent

        commandGraph->addChild(model);
    }

    commandGraph->addChild(createRawSceneData(searchPaths));

    return commandGraph;
}
