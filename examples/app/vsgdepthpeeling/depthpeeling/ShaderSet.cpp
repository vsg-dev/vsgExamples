#include "ShaderSet.h"

#include <vsg/io/read.h>
#include <vsg/state/material.h>

using namespace vsg::oit::depthpeeling;
using namespace vsg;

ref_ptr<ShaderSet> createShadingShaderSet(ShadingModel model, ref_ptr<Options> options)
{
    constexpr auto ViewDescriptorSet = 0;
    constexpr auto MaterialDescriptorSet = 1;
    constexpr auto PeelDescriptorSet = 2;

    auto vertexShader = read_cast<ShaderStage>("shaders/standard.vert", options);
    auto fragmentShader = read_cast<ShaderStage>(model == ShadingModel::Phong
        ? "shaders/dp_pass_phong.frag"
        : "shaders/dp_pass_flat.frag",
        options);

    auto shaderSet = ShaderSet::create(ShaderStages{ vertexShader, fragmentShader });

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT,
        vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT,
        vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord0", "VSG_TEXTURECOORD_0", 2,
        VK_FORMAT_R32G32_SFLOAT, vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord1", "VSG_TEXTURECOORD_1", 3,
        VK_FORMAT_R32G32_SFLOAT, vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord2", "VSG_TEXTURECOORD_2", 4,
        VK_FORMAT_R32G32_SFLOAT, vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord3", "VSG_TEXTURECOORD_3", 5,
        VK_FORMAT_R32G32_SFLOAT, vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 6, VK_FORMAT_R32G32B32A32_SFLOAT,
        vec4Array::create(1), CoordinateSpace::LINEAR);

    shaderSet->addAttributeBinding("vsg_Translation_scaleDistance", "VSG_BILLBOARD", 7,
        VK_FORMAT_R32G32B32A32_SFLOAT, vec4Array::create(1));

    shaderSet->addAttributeBinding("vsg_Translation", "VSG_INSTANCE_TRANSLATION", 7,
        VK_FORMAT_R32G32B32_SFLOAT, vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Rotation", "VSG_INSTANCE_ROTATION", 8,
        VK_FORMAT_R32G32B32A32_SFLOAT, quatArray::create(1));
    shaderSet->addAttributeBinding("vsg_Scale", "VSG_INSTANCE_SCALE", 9,
        VK_FORMAT_R32G32B32_SFLOAT, vec3Array::create(1));

    shaderSet->addAttributeBinding("vsg_JointIndices", "VSG_SKINNING", 10,
        VK_FORMAT_R32G32B32A32_SINT, ivec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_JointWeights", "VSG_SKINNING", 11,
        VK_FORMAT_R32G32B32A32_SFLOAT, vec4Array::create(1));

    shaderSet->addDescriptorBinding(
        "diffuseMap", "VSG_DIFFUSE_MAP", MaterialDescriptorSet, 0,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
        ubvec4Array2D::create(1, 1, Data::Properties{ VK_FORMAT_R8G8B8A8_UNORM }));
    shaderSet->addDescriptorBinding(
        "detailMap", "VSG_DETAIL_MAP", MaterialDescriptorSet, 1,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
        ubvec4Array2D::create(1, 1, Data::Properties{ VK_FORMAT_R8G8B8A8_UNORM }));

    if (model == ShadingModel::Phong)
    {
        shaderSet->addDescriptorBinding(
            "normalMap", "VSG_NORMAL_MAP", MaterialDescriptorSet, 2,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
            vec3Array2D::create(1, 1, Data::Properties{ VK_FORMAT_R32G32B32_SFLOAT }),
            CoordinateSpace::LINEAR);
        shaderSet->addDescriptorBinding(
            "aoMap", "VSG_LIGHTMAP_MAP", MaterialDescriptorSet, 3,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
            vec4Array2D::create(1, 1, Data::Properties{ VK_FORMAT_R32_SFLOAT }));
        shaderSet->addDescriptorBinding(
            "emissiveMap", "VSG_EMISSIVE_MAP", MaterialDescriptorSet, 4,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
            vec4Array2D::create(1, 1, Data::Properties{ VK_FORMAT_R8G8B8A8_UNORM }));
    }

    shaderSet->addDescriptorBinding(
        "displacementMap", "VSG_DISPLACEMENT_MAP", MaterialDescriptorSet, 7,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT,
        floatArray2D::create(1, 1, Data::Properties{ VK_FORMAT_R32_SFLOAT }),
        CoordinateSpace::LINEAR);
    shaderSet->addDescriptorBinding("displacementMapScale", "VSG_DISPLACEMENT_MAP",
        MaterialDescriptorSet, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        1, VK_SHADER_STAGE_VERTEX_BIT,
        vec3Value::create(1.0f, 1.0f, 1.0f));

    shaderSet->addDescriptorBinding(
        "material", "", MaterialDescriptorSet, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
        VK_SHADER_STAGE_FRAGMENT_BIT, PhongMaterialValue::create(), CoordinateSpace::LINEAR);
    shaderSet->addDescriptorBinding(
        "texCoordIndices", "", MaterialDescriptorSet, 11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
        VK_SHADER_STAGE_FRAGMENT_BIT, TexCoordIndicesValue::create(), CoordinateSpace::LINEAR);

    shaderSet->addDescriptorBinding("jointMatrices", "VSG_SKINNING", MaterialDescriptorSet, 12,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
        VK_SHADER_STAGE_VERTEX_BIT, mat4Value::create());

    shaderSet->addDescriptorBinding(
        "lightData", "", ViewDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vec4Array::create(64));
    shaderSet->addDescriptorBinding("viewportData", "", ViewDescriptorSet, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        vec4Value::create(.0f, .0f, 1280.0f, 1024.0f));
    shaderSet->addDescriptorBinding(
        "shadowMaps", "", ViewDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        floatArray3D::create(1, 1, 1, Data::Properties{ VK_FORMAT_R32_SFLOAT }));

    if (model == ShadingModel::Phong)
    {
        shaderSet->addDescriptorBinding("shadowMapDirectSampler", "VSG_SHADOWS_PCSS",
            ViewDescriptorSet, 3, VK_DESCRIPTOR_TYPE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
        shaderSet->addDescriptorBinding("shadowMapShadowSampler", "", ViewDescriptorSet, 4,
            VK_DESCRIPTOR_TYPE_SAMPLER, 1,
            VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    }

    shaderSet->addDescriptorBinding("opaqueDepth", "DEPTHPEELING_PASS", PeelDescriptorSet, 0,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1,
        VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding("prevPassDepth", "DEPTHPEELING_PASS", PeelDescriptorSet, 1,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1,
        VK_SHADER_STAGE_FRAGMENT_BIT, {});

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_ALL, 0, 128);

    if (model == ShadingModel::Phong)
    {
        shaderSet->optionalDefines = { "VSG_GREYSCALE_DIFFUSE_MAP",
                                      "VSG_TWO_SIDED_LIGHTING",
                                      "VSG_POINT_SPRITE",
                                      "VSG_SHADOWS_PCSS",
                                      "VSG_SHADOWS_SOFT",
                                      "VSG_SHADOWS_HARD",
                                      "SHADOWMAP_DEBUG" };
    }
    else
    {
        shaderSet->optionalDefines = { "VSG_POINT_SPRITE", "VSG_GREYSCALE_DIFFUSE_MAP" };
    }
    shaderSet->optionalDefines.insert("DEPTHPEELING_PASS");
    shaderSet->optionalDefines.insert("DEPTHPEELING_FIRSTPASS");

    shaderSet->definesArrayStates.push_back(
        DefinesArrayState{ {"VSG_INSTANCE_TRANSLATION"}, TranslationArrayState::create() });
    shaderSet->definesArrayStates.push_back(DefinesArrayState{
        {"VSG_INSTANCE_TRANSLATION", "VSG_INSTANCE_ROTATION", "VSG_INSTANCE_SCALE"},
        TranslationRotationScaleArrayState::create() });
    shaderSet->definesArrayStates.push_back(
        DefinesArrayState{ {"VSG_INSTANCE_TRANSLATION", "VSG_DISPLACEMENT_MAP"},
                          TranslationAndDisplacementMapArrayState::create() });
    shaderSet->definesArrayStates.push_back(
        DefinesArrayState{ {"VSG_DISPLACEMENT_MAP"}, DisplacementMapArrayState::create() });
    shaderSet->definesArrayStates.push_back(
        DefinesArrayState{ {"VSG_BILLBOARD"}, BillboardArrayState::create() });

    shaderSet->customDescriptorSetBindings.push_back(
        ViewDependentStateBinding::create(ViewDescriptorSet));

    shaderSet->defaultShaderHints = ShaderCompileSettings::create();
    if (model == ShadingModel::Phong)
    {
        shaderSet->defaultShaderHints->defines.insert("VSG_TWO_SIDED_LIGHTING");
    }
    for (auto& stage : shaderSet->stages)
    {
        stage->module->hints = shaderSet->defaultShaderHints;
    }

    return shaderSet;
}

ref_ptr<ShaderSet> vsg::oit::depthpeeling::getOrCreateShadingShaderSet(ShadingModel model, ref_ptr<Options> options)
{
    std::string shaderSetName{ model == ShadingModel::Flat ? FlatShadingModelShaderSetName : PhongShadingModelShaderSetName };

    // check if a ShaderSet has already been assigned to the options object, if so return it
    if (options)
    {
        if (auto itr = options->shaderSets.find(shaderSetName); itr != options->shaderSets.end()) 
        {
            return itr->second;
        }
    }

    // if not found then create a new ShaderSet, assign it to the options object and return it
    auto shaderSet = createShadingShaderSet(model, options);

    if (options)
    {
        options->shaderSets[shaderSetName] = shaderSet;
    }

    return shaderSet;
}

ref_ptr<ShaderSet> createCombineShaderSet(ref_ptr<Options> options)
{
    constexpr auto PeelDescriptorSet = 2;

    auto vertexShader = read_cast<ShaderStage>("shaders/dp_fullscreen.vert", options);
    auto fragmentShader = read_cast<ShaderStage>("shaders/dp_combine.frag", options);

    auto shaderSet = ShaderSet::create(ShaderStages{ vertexShader, fragmentShader });

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT,
        vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT,
        vec3Array::create(1));

    shaderSet->addDescriptorBinding("peelOutput", "", PeelDescriptorSet, 2,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1,
        VK_SHADER_STAGE_FRAGMENT_BIT, {});

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_ALL, 0, 128);

    shaderSet->optionalDefines = { "DEPTHPEELING_PASS" };

    shaderSet->defaultShaderHints = ShaderCompileSettings::create();
    for (auto& stage : shaderSet->stages)
    {
        stage->module->hints = shaderSet->defaultShaderHints;
    }

    return shaderSet;
}

ref_ptr<ShaderSet> vsg::oit::depthpeeling::getOrCreateCombineShaderSet(ref_ptr<Options> options)
{
    // check if a ShaderSet has already been assigned to the options object, if so return it
    if (options)
    {
        if (auto itr = options->shaderSets.find(CombineShaderSetName); itr != options->shaderSets.end())
        {
            return itr->second;
        }
    }

    // if not found then create a new ShaderSet, assign it to the options object and return it
    auto shaderSet = createCombineShaderSet(options);

    if (options)
    {
        options->shaderSets[CombineShaderSetName] = shaderSet;
    }

    return shaderSet;
}
