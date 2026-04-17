#include "PipelineConfigurator.h"

using namespace vsg::oit::depthpeeling;
using namespace vsg;

struct SetPeelCombinePipelineStates : public Visitor
{
    bool underblending = true;

    SetPeelCombinePipelineStates(bool underblend)
        : underblending{ underblend }
    {
    }

    void apply(Object& object) override
    {
        object.traverse(*this);
    }

    void apply(ColorBlendState& cbs) override
    {
        for (auto& blendAttachment : cbs.attachments)
        {
            blendAttachment.blendEnable = VK_TRUE;

            if (underblending)
            {
                // -------------------------------
                // Under-Blending
                // -------------------------------

                // Color: dst.rgb + src.rgb * (1 - dst.a)
                blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
                blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

                // Alpha: dst.a + src.a * (1 - dst.a)
                blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
                blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            }
            else
            {
                // -------------------------------
                // Over-Blending
                // -------------------------------

                // Color: src.rgb + dst.rgb * (1 - src.a)
                blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

                // Alpha: src.a + dst.a * (1 - src.a)
                blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            }

            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }
    }

    void apply(RasterizationState& rs) override
    {
        // disable culling mode completely for blending passes
        rs.cullMode = VK_CULL_MODE_NONE;
    }

    void apply(DepthStencilState& dss) override
    {
        // disable depth buffer completely for blending passes
        dss.depthTestEnable = VK_FALSE;
        dss.depthWriteEnable = VK_FALSE;
    }
};

void configureSharedBindings(GraphicsPipelineConfigurator& configurator, ref_ptr<Options>& options)
{
    if (options.valid())
    {
        configurator.assignInheritedState(options->inheritedState);
    }

    configurator.enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    configurator.enableArray("vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    configurator.enableArray("vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, 8);
    configurator.enableArray("vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, 4);

    configurator.enableDescriptor("material");
    configurator.enableDescriptor("texCoordIndices");

    configurator.enableTexture("diffuseMap");

    if (auto* descriptorConfigurator = configurator.descriptorConfigurator.get(); descriptorConfigurator != nullptr)
    {
        descriptorConfigurator->assignDefaults(configurator.inheritedSets);
        configurator.shaderHints->defines.insert(
            descriptorConfigurator->defines.begin(), descriptorConfigurator->defines.end());
    }
}

void initializePipelineConfigurator(vsg::ref_ptr<GraphicsPipelineConfigurator>& configurator, ref_ptr<Options>& options)
{
    if (options.valid() && options->sharedObjects.valid())
    {
        options->sharedObjects->share(configurator, [](auto gpc) { gpc->init(); });
    }
    else
    {
        configurator->init();
    }
}

ref_ptr<GraphicsPipelineConfigurator> OpaquePipelineConfigurator::getOrCreate(ShadingModel model, ref_ptr<Options> options)
{
    auto configurator = GraphicsPipelineConfigurator::create(getOrCreateShadingShaderSet(model, options));
    configureSharedBindings(*configurator, options);

    initializePipelineConfigurator(configurator, options);
    return configurator;
}

vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> PeelPipelineConfigurator::getOrCreate(
    ShadingModel model, uint32_t peelIndex, vsg::ref_ptr<vsg::Options> options)
{
    auto configurator = GraphicsPipelineConfigurator::create(getOrCreateShadingShaderSet(model, options));
    configurator->subpass = peelIndex * 2;
    configureSharedBindings(*configurator, options);

    configurator->shaderHints->defines.insert("DEPTHPEELING_PASS");
    if (peelIndex == 0)
    {
        configurator->shaderHints->defines.insert("DEPTHPEELING_FIRSTPASS");
    }

    initializePipelineConfigurator(configurator, options);
    return configurator;
}

vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> PeelCombinePipelineConfigurator::getOrCreate(
    uint32_t peelIndex, vsg::ref_ptr<vsg::Options> options)
{
    auto configurator = GraphicsPipelineConfigurator::create(getOrCreateCombineShaderSet(options));
    configurator->subpass = peelIndex * 2 + 1;
    if (options.valid())
    {
        configurator->assignInheritedState(options->inheritedState);
    }

    configurator->shaderHints->defines.insert("DEPTHPEELING_PASS");
    
    SetPeelCombinePipelineStates sps{ true };
    configurator->accept(sps);

    initializePipelineConfigurator(configurator, options);
    return configurator;
}

vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> FinalCombinePipelineConfigurator::getOrCreate(
    uint32_t numPeelLayers, vsg::ref_ptr<vsg::Options> options)
{
    auto configurator = GraphicsPipelineConfigurator::create(getOrCreateCombineShaderSet(options));
    configurator->subpass = numPeelLayers * 2;
    if (options.valid())
    {
        configurator->assignInheritedState(options->inheritedState);
    }

    SetPeelCombinePipelineStates sps{ false };
    configurator->accept(sps);

    initializePipelineConfigurator(configurator, options);
    return configurator;
}
