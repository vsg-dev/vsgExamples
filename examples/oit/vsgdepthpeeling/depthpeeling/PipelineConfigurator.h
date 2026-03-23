#pragma once

#include "ShaderSet.h"

#include <vsg/utils/GraphicsPipelineConfigurator.h>

namespace vsg::oit::depthpeeling {

    class OpaquePipelineConfigurator
    {
    public:
        OpaquePipelineConfigurator() = delete;

        static vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> getOrCreate(
            ShadingModel model, vsg::ref_ptr<vsg::Options> options);
    };

    class PeelPipelineConfigurator
    {
    public:
        PeelPipelineConfigurator() = delete;

        static vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> getOrCreate(
            ShadingModel model, uint32_t peelIndex, vsg::ref_ptr<vsg::Options> options);
    };

    class PeelCombinePipelineConfigurator
    {
    public:
        PeelCombinePipelineConfigurator() = delete;

        static vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> getOrCreate(
            uint32_t peelIndex, vsg::ref_ptr<vsg::Options> options);
    };

    class FinalCombinePipelineConfigurator
    {
    public:
        FinalCombinePipelineConfigurator() = delete;

        static vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> getOrCreate(
            uint32_t numPeelLayers, vsg::ref_ptr<vsg::Options> options);
    };

}
