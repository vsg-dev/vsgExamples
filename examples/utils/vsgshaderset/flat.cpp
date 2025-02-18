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
#include <vsg/state/ColorBlendState.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/InputAssemblyState.h>
#include <vsg/state/MultisampleState.h>
#include <vsg/state/RasterizationState.h>
#include <vsg/state/TessellationState.h>
#include <vsg/state/VertexInputState.h>
#include <vsg/state/material.h>
#include <vsg/utils/ShaderSet.h>

vsg::ref_ptr<vsg::ShaderSet> flat_ShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Local flat_ShaderSet(", options, ")");

    auto vertexShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard.vert", options);
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>("shaders/standard_flat_shaded.frag", options);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("flat_ShaderSet(...) could not find shaders.");
        return {};
    }

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_TexCoord0", "", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    shaderSet->addAttributeBinding("vsg_Color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1), vsg::CoordinateSpace::LINEAR);

    shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("vsg_position_scaleDistance", "VSG_BILLBOARD", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    shaderSet->addAttributeBinding("vsg_JointIndices", "VSG_SKINNING", 5, VK_FORMAT_R32G32B32A32_SINT, vsg::ivec4Array::create(1));
    shaderSet->addAttributeBinding("vsg_JointWeights", "VSG_SKINNING", 6, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    shaderSet->addDescriptorBinding("detailMap", "VSG_DETAIL_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));

    shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}), vsg::CoordinateSpace::LINEAR);
    shaderSet->addDescriptorBinding("displacementMapScale", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec3Value::create(1.0f, 1.0f, 1.0f));

    shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create(), vsg::CoordinateSpace::LINEAR);

    shaderSet->addDescriptorBinding("jointMatrices", "VSG_SKINNING", MATERIAL_DESCRIPTOR_SET, 11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::mat4Value::create());

    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0, 0, 1280, 1024));
    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_ALL, 0, 128);

    shaderSet->optionalDefines = {"VSG_POINT_SPRITE", "VSG_GREYSCALE_DIFFUSE_MAP", "VSG_ALPHA_TEST"};

    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS", "VSG_DISPLACEMENT_MAP"}, vsg::PositionAndDisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS"}, vsg::PositionArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});

    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

    return shaderSet;
}
