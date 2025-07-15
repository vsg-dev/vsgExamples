/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Array2D.h>
#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/material.h>
#include <vsg/utils/ShaderSet.h>

#include "custom_pbr.h"

vsg::RegisterWithObjectFactoryProxy<custom::FogValue> s_Register_FogValue;

vsg::ref_ptr<vsg::ShaderSet> custom::pbr_ShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Local pbr_ShaderSet(", options, ")");

    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/custom_pbr.vert", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/custom_pbr.frag", options);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("pbr_ShaderSet(...) could not find shaders.");
        return {};
    }

#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2
#define CUSTOM_DESCRIPTOR_SET 0

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord0", "VSG_TEXTURECOORD_0", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord1", "VSG_TEXTURECOORD_1", 3, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord2", "VSG_TEXTURECOORD_2", 4, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord3", "VSG_TEXTURECOORD_3", 5, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 6, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1), vsg::CoordinateSpace::LINEAR);

    shaderSet->addAttributeBinding("vsg_Translation_scaleDistance", "VSG_BILLBOARD", 7, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    shaderSet->addAttributeBinding("vsg_Translation", "VSG_INSTANCE_TRANSLATION", 7, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Rotation", "VSG_INSTANCE_ROTATION", 8, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::quatArray::create(1));
    shaderSet->addAttributeBinding("vsg_Scale", "VSG_INSTANCE_SCALE", 9, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    shaderSet->addAttributeBinding("vsg_JointIndices", "VSG_SKINNING", 10, VK_FORMAT_R32G32B32A32_SINT, vsg::ivec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_JointWeights", "VSG_SKINNING", 11, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("detailMap", "VSG_DETAIL_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("normalMap", "VSG_NORMAL_MAP", MATERIAL_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32_SFLOAT}), vsg::CoordinateSpace::LINEAR);
    shaderSet->addDescriptorBinding("aoMap", "VSG_LIGHTMAP_MAP", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->addDescriptorBinding("emissiveMap", "VSG_EMISSIVE_MAP", MATERIAL_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("specularMap", "VSG_SPECULAR_MAP", MATERIAL_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("mrMap", "VSG_METALLROUGHNESS_MAP", MATERIAL_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec2Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32_SFLOAT}), vsg::CoordinateSpace::LINEAR);

    shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}), vsg::CoordinateSpace::LINEAR);
    shaderSet->addDescriptorBinding("displacementMapScale", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec3Value::create(1.0f, 1.0f, 1.0f));

    shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PbrMaterialValue::create(), vsg::CoordinateSpace::LINEAR);
    shaderSet->addDescriptorBinding("texCoordIndices", "", MATERIAL_DESCRIPTOR_SET, 11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::TexCoordIndicesValue::create(), vsg::CoordinateSpace::LINEAR);

    shaderSet->addDescriptorBinding("jointMatrices", "VSG_SKINNING", MATERIAL_DESCRIPTOR_SET, 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::mat4Value::create());

    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0, 0, 1280, 1024));
    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    shaderSet->addDescriptorBinding("shadowMapDirectSampler", "VSG_SHADOWS_PCSS", VIEW_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    shaderSet->addDescriptorBinding("shadowMapShadowSampler", "", VIEW_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);

    shaderSet->addDescriptorBinding("Fog", "", CUSTOM_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, custom::FogValue::create());

    // additional defines
    shaderSet->optionalDefines = {"VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_POINT_SPRITE", "VSG_WORKFLOW_SPECGLOSS", "VSG_SHADOWS_PCSS", "VSG_SHADOWS_SOFT", "VSG_SHADOWS_HARD", "SHADOWMAP_DEBUG", "VSG_ALPHA_TEST"};

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_ALL, 0, 128);

    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION"}, vsg::TranslationArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION", "VSG_INSTANCE_ROTATION", "VSG_INSTANCE_SCALE"}, vsg::TranslationRotationScaleArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_TRANSLATION", "VSG_DISPLACEMENT_MAP"}, vsg::TranslationAndDisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});

    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

    return shaderSet;
}
