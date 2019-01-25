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
    VSG_type_name(vsg::GraphicsNode);

    class CompileTraversal : public Visitor
    {
    public:

        CompileTraversal() {}

        void apply(Group& group);
        void apply(GraphicsNode& graphics);

        Context context;
    };

    class UpdatePipeline : public vsg::Visitor
    {
    public:

        vsg::ref_ptr<vsg::ViewportState> _viewportState;

        UpdatePipeline(vsg::ViewportState* viewportState) :
            _viewportState(viewportState) {}

        void apply(vsg::BindPipeline& bindPipeline)
        {
            std::cout<<"Found BindPipeline "<<std::endl;
            vsg::GraphicsPipeline* graphicsPipeline = dynamic_cast<vsg::GraphicsPipeline*>(bindPipeline.getPipeline());
            if (graphicsPipeline)
            {
                bool needToRegenerateGraphicsPipeline = false;
                for(auto& pipelineState : graphicsPipeline->getPipelineStates())
                {
                    if (pipelineState==_viewportState)
                    {
                        needToRegenerateGraphicsPipeline = true;
                    }
                }
                if (needToRegenerateGraphicsPipeline)
                {

                    vsg::ref_ptr<vsg::GraphicsPipeline> new_pipeline = vsg::GraphicsPipeline::create(graphicsPipeline->getRenderPass()->getDevice(),
                                                                                                    graphicsPipeline->getRenderPass(),
                                                                                                    graphicsPipeline->getPipelineLayout(),
                                                                                                    graphicsPipeline->getPipelineStates());

                    bindPipeline.setPipeline(new_pipeline);

                    std::cout<<"found matching viewport, replaced."<<std::endl;
                }
            }
        }

        void apply(vsg::Group& stateGroup)
        {
            stateGroup.traverse(*this);
        }
    };

    class GraphicsPipelineGroup : public Inherit<GraphicsNode, GraphicsPipelineGroup>
    {
    public:
        GraphicsPipelineGroup(Allocator* allocator = nullptr);

        using Inherit::traverse;

        void traverse(Visitor& visitor) override;
        void traverse(ConstVisitor& visitor) const override;

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
    VSG_type_name(vsg::GraphicsPipelineGroup);

    class Texture : public Inherit<GraphicsNode, Texture>
    {
    public:
        Texture(Allocator* allocator = nullptr);

        using Inherit::traverse;

        void traverse(Visitor& visitor) override;
        void traverse(ConstVisitor& visitor) const override;

        void accept(DispatchTraversal& dv) const override;

        void compile(Context& context);

        ref_ptr<Data> _textureData;
        ref_ptr<vsg::BindDescriptorSets> _bindDescriptorSets;
    };
    VSG_type_name(vsg::Texture);

    class MatrixTransform : public Inherit<GraphicsNode, MatrixTransform>
    {
    public:
        MatrixTransform(Allocator* allocator = nullptr);

        void accept(DispatchTraversal& dv) const override;

        void compile(Context& context) override;

        ref_ptr<mat4Value> _matrix;

        ref_ptr<PushConstants> _pushConstant;
    };
    VSG_type_name(vsg::MatrixTransform);

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
    VSG_type_name(vsg::Geometry);
}
