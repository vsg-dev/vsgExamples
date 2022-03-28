
#include <vsg/state/GraphicsPipeline.h>
#include <vsg/state/VertexInputState.h>
#include <vsg/state/InputAssemblyState.h>
#include <vsg/state/ColorBlendState.h>
#include <vsg/state/MultisampleState.h>
#include <vsg/state/DepthStencilState.h>
#include <vsg/state/RasterizationState.h>
#include <vsg/state/DescriptorSetLayout.h>

#include "ShaderSet.h"

namespace vsg
{

    class GraphicsPipelineConfig : public vsg::Inherit<Object, GraphicsPipelineConfig>
    {
    public:

            GraphicsPipelineConfig(ref_ptr<ShaderSet> in_shaderSet = {});

            // inputs to setup of GraphicsPipeline
            ref_ptr<VertexInputState> vertexInputState;
            ref_ptr<InputAssemblyState> inputAssemblyState;
            ref_ptr<RasterizationState> rasterizationState;
            ref_ptr<ColorBlendState> colorBlendState;
            ref_ptr<MultisampleState> multisampleState;
            ref_ptr<DepthStencilState> depthStencilState;
            uint32_t subpass = 0;
            uint32_t attributeBindingIndex = 0;

            ref_ptr<ShaderCompileSettings> shaderHints;
            vsg::DescriptorSetLayoutBindings descriptorBindings;
            ref_ptr<ShaderSet> shaderSet;

            const AttributeBinding& getAttributeBinding(const std::string& name, ref_ptr<Data> array, VkVertexInputRate vertexInputRate);
            const UniformBinding& getUniformBinding(const std::string& name);

            // setup GraphicsPipeline
            ref_ptr<DescriptorSetLayout> descriptorSetLayout;
            ref_ptr<PipelineLayout> layout;
            ref_ptr<GraphicsPipeline> graphicsPipeline;
            ref_ptr<BindGraphicsPipeline> bindGraphicsPipeline;

            void init();

            int compare(const Object& rhs) const;
    };

}
