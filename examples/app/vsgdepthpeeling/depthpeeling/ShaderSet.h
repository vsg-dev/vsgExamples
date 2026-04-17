#pragma once

#include "ShaderSet.h"

#include <vsg/utils/ShaderSet.h>
#include <vsg/io/Options.h>

namespace vsg::oit::depthpeeling {

    enum class ShadingModel
    {
        Flat,
        Phong
    };

    constexpr const char* FlatShadingModelShaderSetName = "depthPeeling_flat";
    constexpr const char* PhongShadingModelShaderSetName = "depthPeeling_phong";
    constexpr const char* CombineShaderSetName = "depthPeeling_combine";

    extern ref_ptr<ShaderSet> getOrCreateShadingShaderSet(ShadingModel model, ref_ptr<Options> options = {});
    extern ref_ptr<ShaderSet> getOrCreateCombineShaderSet(ref_ptr<Options> options = {});

}
