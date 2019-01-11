#include <vsg/vk/BufferData.h>
#include <vsg/vk/Descriptor.h>
#include <vsg/vk/DescriptorSet.h>
#include <vsg/vk/DescriptorPool.h>
#include <vsg/vk/BindVertexBuffers.h>
#include <vsg/vk/BindIndexBuffer.h>
#include <vsg/vk/Draw.h>

#include <iostream>

#include "CreateSceneData.h"


SceneData createSceneData(vsg::ref_ptr<vsg::Device> device, vsg::ref_ptr<vsg::CommandPool> commandPool, vsg::ref_ptr<vsg::RenderPass> renderPass, VkQueue graphicsQueue, // viewer/window
                          vsg::ref_ptr<vsg::mat4Value> projMatrix, vsg::ref_ptr<vsg::mat4Value> viewMatrix, vsg::ref_ptr<vsg::ViewportState> viewport, // camera
                          vsg::ref_ptr<vsg::Shader> vertexShader, vsg::ref_ptr<vsg::Shader> fragmentShader, vsg::ref_ptr<vsg::Data> textureData) // scene graph
{
    SceneData scenedata;

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


    //
    // set up texture image
    //
    vsg::ImageData imageData = vsg::transferImageData(device, commandPool, graphicsQueue, textureData);
    if (!imageData.valid())
    {
        std::cout<<"Texture not created"<<std::endl;
        return scenedata;
    }

    vsg::ref_ptr<vsg::mat4Value> modelMatrix(new vsg::mat4Value);

    //
    // set up descriptor layout and descriptor set and pieline layout for uniforms
    //
    vsg::ref_ptr<vsg::DescriptorPool> descriptorPool = vsg::DescriptorPool::create(device, 1,
    {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    });

    vsg::ref_ptr<vsg::PushConstants> pushConstant_proj = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 0, projMatrix);
    vsg::ref_ptr<vsg::PushConstants> pushConstant_view = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 64, viewMatrix);
    vsg::ref_ptr<vsg::PushConstants> pushConstant_model = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 128, modelMatrix);

    vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout = vsg::DescriptorSetLayout::create(device,
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    });

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196}
    };

    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(device, descriptorPool, descriptorSetLayout,
    {
        vsg::DescriptorImage::create(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{imageData})
    });

    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout = vsg::PipelineLayout::create(device, {descriptorSetLayout}, pushConstantRanges);

    // setup binding of descriptors
    vsg::ref_ptr<vsg::BindDescriptorSets> bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, vsg::DescriptorSets{descriptorSet}); // device dependent


    // set up graphics pipeline
    vsg::VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0},
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},
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
        viewport, // device independent
        vsg::RasterizationState::create(),// device independent
        vsg::MultisampleState::create(),// device independent
        vsg::ColorBlendState::create(),// device independent
        vsg::DepthStencilState::create()// device independent
    });

    scenedata.pipeline = pipeline; // need to pass this back to allow updates to GraphicsPipeline on resize

    vsg::ref_ptr<vsg::BindPipeline> bindPipeline = vsg::BindPipeline::create(pipeline);

    scenedata.bindPipeline = bindPipeline;

    // set up vertex buffer binding
    vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);  // device dependent

    // set up index buffer binding
    vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(indexBufferData.front(), VK_INDEX_TYPE_UINT16); // device dependent

    // set up drawing of the triangles
    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(12, 1, 0, 0, 0); // device independent

    // set up what we want to render in a command graph
    // create command graph to contain all the Vulkan calls for specifically rendering the model
    auto commandGraph = vsg::StateGroup::create();
    scenedata.commandGraph = commandGraph;

    // set up the state configuration
    commandGraph->add(bindPipeline);  // device dependent
    commandGraph->add(bindDescriptorSets);  // device dependent

    commandGraph->add(pushConstant_proj);
    commandGraph->add(pushConstant_view);
    commandGraph->add(pushConstant_model);

    // add subgraph that represents the model to render
    vsg::ref_ptr<vsg::StateGroup> model = vsg::StateGroup::create();
    commandGraph->addChild(model);

    // add the vertex and index buffer data
    model->add(bindVertexBuffers); // device dependent
    model->add(bindIndexBuffer); // device dependent

    // add the draw primitive command
    model->addChild(drawIndexed); // device independent

    return scenedata;
}
