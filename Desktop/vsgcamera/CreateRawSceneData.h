#pragma once

#include <vsg/nodes/Node.h>

#include <vsg/vk/GraphicsPipeline.h>
#include <vsg/vk/PushConstants.h>
#include <vsg/vk/CommandPool.h>


namespace vsg
{
    struct Context
    {
        ref_ptr<Device> device;
        ref_ptr<CommandPool> commandPool;
        ref_ptr<RenderPass> renderPass;
        ref_ptr<ViewportState> viewport;
        VkQueue graphicsQueue = 0;

        ref_ptr<DescriptorPool> descriptorPool;
        ref_ptr<DescriptorSetLayout> descriptorSetLayout;
        ref_ptr<PipelineLayout> pipelineLayout;

        ref_ptr<mat4Value> projMatrix;
        ref_ptr<mat4Value> viewMatrix;
    };


    class GraphicsNode : public Inherit<Group, GraphicsNode>
    {
    public:
        GraphicsNode(Allocator* allocator = nullptr):
            Inherit(allocator) {}

        virtual void compile(Context& context) = 0;
    };

    // one set per thread?
    struct FrameResources
    {
        using DescriptorPools = std::vector<ref_ptr<DescriptorPool>>;
        using DescriptorSetLayouts = std::vector<ref_ptr<DescriptorSetLayout>>;
        using PiplineLayouts = std::vector<ref_ptr<PipelineLayout>>;
        using GraphicsPipelines = std::vector<ref_ptr<GraphicsPipeline>>;

    };

    class GraphicsPipelineGroup : public Inherit<GraphicsNode, GraphicsPipelineGroup>
    {
    public:
        GraphicsPipelineGroup(Allocator* allocator = nullptr);

        void init();

        void accept(DispatchTraversal& dv) const override;

        void compile(Context& context) override;

        using Shaders = std::vector<ref_ptr<Shader>>;

        // descriptorPool ..
        uint32_t maxSets = 0;
        DescriptorPoolSizes descriptorPoolSizes; // need to accumulate descriptorPoolSizes by looking at scene graph
        // descriptorSetLayout ..
        DescriptorSetLayoutBindings descriptorSetLayoutBindings;
        PushConstantRanges pushConstantRanges;
        VertexInputState::Bindings vertexBindingsDescriptions;
        VertexInputState::Attributes vertexAttributeDescriptions;
        Shaders shaders;
        GraphicsPipelineStates pipelineStates;

        ref_ptr<BindPipeline> _bindPipeline;
        ref_ptr<PushConstants> _projPushConstant;
        ref_ptr<PushConstants> _viewPushConstant;
    };


    class Texture : public Inherit<GraphicsNode, Texture>
    {
    public:
        Texture(Allocator* allocator = nullptr);

        void compile(Context& context);

        ref_ptr<Data> _textureData;
        ref_ptr<vsg::BindDescriptorSets> _bindDescriptorSets;
    };

    class MatrixTransform : public Inherit<GraphicsNode, MatrixTransform>
    {
    public:
        MatrixTransform(Allocator* allocator = nullptr);

        void accept(DispatchTraversal& dv) const override;

        void compile(Context& context) override;

        ref_ptr<mat4Value> _matrix;

        ref_ptr<PushConstants> _pushConstant;
    };

    class Geometry : public Inherit<GraphicsNode, Geometry>
    {
    public:
        Geometry(Allocator* allocator = nullptr);

        void accept(DispatchTraversal& dv) const override;

        void compile(Context& context) override;

        using Commands = std::vector<ref_ptr<Command>>;

        DataList _arrays;
        ref_ptr<Data> _indices;
        Commands _commands;

        ref_ptr<Group> _renderImplementation;


    };

    class CompileTraversal : public Visitor
    {
    public:

        CompileTraversal() {}

        void apply(Group& group);
        void apply(GraphicsNode& graphics);

        Context context;
    };
}

extern vsg::ref_ptr<vsg::Node> createRawSceneData(vsg::Paths& searchPaths);
