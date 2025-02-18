#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include "vsg/all.h"
#include <vsgXchange/all.h>


struct UBO
{
    vsg::vec4 lightPos;
    vsg::vec4 frustumPlanes[6];
    float displacementFactor;
    float tessellationFactor;
    vsg::vec2 viewportDim;
    float tessellatedEdgeSize;
};

using UBOValue = vsg::Value<UBO>;


std::string TESC{R"(
#version 450

layout(push_constant) uniform PushConstants { mat4 projection; mat4 modelView; };

layout(set = 0, binding = 0) uniform UBO
{
	vec4 lightPos;
	vec4 frustumPlanes[6];
	float displacementFactor;
	float tessellationFactor;
	vec2 viewportDim;
	float tessellatedEdgeSize;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D samplerHeight;

layout (vertices = 4) out;
 
layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inUV[];
 
layout (location = 0) out vec3 outNormal[4];
layout (location = 1) out vec2 outUV[4];
 
// Calculate the tessellation factor based on screen space
// dimensions of the edge
float screenSpaceTessFactor(vec4 p0, vec4 p1)
{
	// Calculate edge mid point
	vec4 midPoint = 0.5 * (p0 + p1);
	// Sphere radius as distance between the control points
	float radius = distance(p0, p1) / 2.0;

	// View space
	vec4 v0 = modelView  * midPoint;

	// Project into clip space
	vec4 clip0 = (projection * (v0 - vec4(radius, vec3(0.0))));
	vec4 clip1 = (projection * (v0 + vec4(radius, vec3(0.0))));

	// Get normalized device coordinates
	clip0 /= clip0.w;
	clip1 /= clip1.w;

	// Convert to viewport coordinates
	clip0.xy *= ubo.viewportDim;
	clip1.xy *= ubo.viewportDim;
	
	// Return the tessellation factor based on the screen size 
	// given by the distance of the two edge control points in screen space
	// and a reference (min.) tessellation size for the edge set by the application
	return clamp(distance(clip0, clip1) / ubo.tessellatedEdgeSize * ubo.tessellationFactor, 1.0, 64.0);
}

// Checks the current's patch visibility against the frustum using a sphere check
// Sphere radius is given by the patch size
bool frustumCheck()
{
	// Fixed radius (increase if patch size is increased in example)
	const float radius = 8.0f;
	vec4 pos = gl_in[gl_InvocationID].gl_Position;
	pos.z += textureLod(samplerHeight, inUV[0], 0.0).r * ubo.displacementFactor;

	// Check sphere against frustum planes
	for (int i = 0; i < 6; i++) {
		if (dot(pos, ubo.frustumPlanes[i]) + radius < 0.0)
		{
			return false;
		}
	}
	return true;
}

void main()
{
	if (gl_InvocationID == 0)
	{
		if (!frustumCheck())
		{
			gl_TessLevelInner[0] = 0.0;
			gl_TessLevelInner[1] = 0.0;
			gl_TessLevelOuter[0] = 0.0;
			gl_TessLevelOuter[1] = 0.0;
			gl_TessLevelOuter[2] = 0.0;
			gl_TessLevelOuter[3] = 0.0;
		}
		else
		{
			if (ubo.tessellationFactor > 0.0)
			{
				gl_TessLevelOuter[0] = screenSpaceTessFactor(gl_in[3].gl_Position, gl_in[0].gl_Position);
				gl_TessLevelOuter[1] = screenSpaceTessFactor(gl_in[0].gl_Position, gl_in[1].gl_Position);
				gl_TessLevelOuter[2] = screenSpaceTessFactor(gl_in[1].gl_Position, gl_in[2].gl_Position);
				gl_TessLevelOuter[3] = screenSpaceTessFactor(gl_in[2].gl_Position, gl_in[3].gl_Position);
				gl_TessLevelInner[0] = mix(gl_TessLevelOuter[0], gl_TessLevelOuter[3], 0.5);
				gl_TessLevelInner[1] = mix(gl_TessLevelOuter[2], gl_TessLevelOuter[1], 0.5);
			}
			else
			{
				// Tessellation factor can be set to zero by example
				// to demonstrate a simple passthrough
				gl_TessLevelInner[0] = 1.0;
				gl_TessLevelInner[1] = 1.0;
				gl_TessLevelOuter[0] = 1.0;
				gl_TessLevelOuter[1] = 1.0;
				gl_TessLevelOuter[2] = 1.0;
				gl_TessLevelOuter[3] = 1.0;
			}
		}

	}

	gl_out[gl_InvocationID].gl_Position =  gl_in[gl_InvocationID].gl_Position;
	outNormal[gl_InvocationID] = inNormal[gl_InvocationID];
	outUV[gl_InvocationID] = inUV[gl_InvocationID];
} 
)"};

std::string TESE{R"(
#version 450

layout(push_constant) uniform PushConstants { mat4 projection; mat4 modelView; };

layout (set = 0, binding = 0) uniform UBO 
{
	vec4 lightPos;
	vec4 frustumPlanes[6];
	float displacementFactor;
	float tessellationFactor;
	vec2 viewportDim;
	float tessellatedEdgeSize;
} ubo; 

layout (set = 0, binding = 1) uniform sampler2D displacementMap; 

layout(quads, equal_spacing, cw) in;

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inUV[];
 
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec3 outEyePos;
layout (location = 5) out vec3 outWorldPos;

void main()
{
	// Interpolate UV coordinates
	vec2 uv1 = mix(inUV[0], inUV[1], gl_TessCoord.x);
	vec2 uv2 = mix(inUV[3], inUV[2], gl_TessCoord.x);
	outUV = mix(uv1, uv2, gl_TessCoord.y);

	vec3 n1 = mix(inNormal[0], inNormal[1], gl_TessCoord.x);
	vec3 n2 = mix(inNormal[3], inNormal[2], gl_TessCoord.x);
	outNormal = mix(n1, n2, gl_TessCoord.y);

	// Interpolate positions
	vec4 pos1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 pos2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(pos1, pos2, gl_TessCoord.y);
	// Displace
	pos.z += textureLod(displacementMap, outUV, 0.0).r * ubo.displacementFactor;
	// Perspective projection
	gl_Position = projection * modelView * pos;

	// Calculate vectors for lighting based on tessellated position
	outViewVec = -pos.xyz;
	outLightVec = normalize(ubo.lightPos.xyz + outViewVec);
	outWorldPos = pos.xyz;
	outEyePos = vec3(modelView * pos);
}
)"};

std::string VERT{R"(
#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;

void main(void)
{
	gl_Position = vec4(inPos.xyz, 1.0);
	outUV = inUV;
	outNormal = inNormal;
}
)"};

std::string FRAG{R"(
#version 450

layout (set = 0, binding = 1) uniform sampler2D samplerHeight; 
layout (set = 0, binding = 2) uniform sampler2DArray samplerLayers;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec3 inEyePos;
layout (location = 5) in vec3 inWorldPos;

layout (location = 0) out vec4 outFragColor;

vec3 sampleTerrainLayer()
{
	// Define some layer ranges for sampling depending on terrain height
	vec2 layers[6];
	layers[0] = vec2(-10.0, 10.0);
	layers[1] = vec2(5.0, 45.0);
	layers[2] = vec2(45.0, 80.0);
	layers[3] = vec2(75.0, 100.0);
	layers[4] = vec2(95.0, 140.0);
	layers[5] = vec2(140.0, 190.0);

	vec3 color = vec3(0.0);
	
	// Get height from displacement map
	float height = textureLod(samplerHeight, inUV, 0.0).r * 255.0;
	
	for (int i = 0; i < 6; i++)
	{
		float range = layers[i].y - layers[i].x;
		float weight = (range - abs(height - layers[i].y)) / range;
		weight = max(0.0, weight);
		color += weight * texture(samplerLayers, vec3(inUV * 16.0, i)).rgb;
	}

	return color;
}

float fog(float density)
{
	const float LOG2 = -1.442695;
	float dist = gl_FragCoord.z / gl_FragCoord.w * 0.1;
	float d = density * dist;
	return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

void main()
{
	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 ambient = vec3(0.5);
	vec3 diffuse = max(dot(N, L), 0.0) * vec3(1.0);

	vec4 color = vec4((ambient + diffuse) * sampleTerrainLayer(), 1.0);

	const vec4 fogColor = vec4(0.47, 0.5, 0.67, 0.0);
	outFragColor  = mix(color, fogColor, fog(0.25));	
}

)"};

vsg::ref_ptr<vsg::BindGraphicsPipeline> createBindGraphicsPipeline()
{
    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings {
        // Binding 0 : Shared Tessellation shader ubo
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
        // Binding 1 : Height map
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        // Binding 2 : Terrain texture array layers
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    };
    
    /// Pipeline Layout: Descriptors (none in this case) and Push Constants
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetLayoutBindings);
    vsg::DescriptorSetLayouts descriptorSetLayouts{descriptorSetLayout};
    vsg::PushConstantRanges pushConstantRanges{{VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0, 128}};
    auto pipelineLayout = vsg::PipelineLayout::create(
        descriptorSetLayouts, pushConstantRanges);

    /// Shaders
    auto verttexShaderHints = vsg::ShaderCompileSettings::create();
    auto vertexShader = vsg::ShaderStage::create(
        VK_SHADER_STAGE_VERTEX_BIT, "main", VERT, verttexShaderHints);
    
    auto fragmentShaderHints = vsg::ShaderCompileSettings::create();
    auto fragmentShader = vsg::ShaderStage::create(
        VK_SHADER_STAGE_FRAGMENT_BIT, "main", FRAG, fragmentShaderHints);
    
    auto tesselationControlShaderHints = vsg::ShaderCompileSettings::create();
    auto tesselationControlShader = vsg::ShaderStage::create(
        VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "main", TESC, tesselationControlShaderHints);

    auto tesselationEvaluationShaderHints = vsg::ShaderCompileSettings::create();
    auto tesselationEvaluationShader = vsg::ShaderStage::create(
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "main", TESE, tesselationEvaluationShaderHints);
    auto shaderStages = vsg::ShaderStages{vertexShader, tesselationControlShader, tesselationEvaluationShader, fragmentShader};

    /// Vertex Attributes
    auto vertexInputState = vsg::VertexInputState::create();
    auto& bindings = vertexInputState->vertexBindingDescriptions;
    auto& attributes = vertexInputState->vertexAttributeDescriptions;

    uint32_t bindingIndex{0};
    uint32_t location{0};
    uint32_t offset{0};

    // position
    bindings.emplace_back(VkVertexInputBindingDescription{
        bindingIndex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX});
    bindingIndex++;

    // normal
    bindings.emplace_back(VkVertexInputBindingDescription{
        bindingIndex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX});
    bindingIndex++;

    // uv
    bindings.emplace_back(VkVertexInputBindingDescription{
        bindingIndex, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX});
    bindingIndex++;

    bindingIndex = 0;
    attributes.emplace_back(VkVertexInputAttributeDescription{
        bindingIndex, location, VK_FORMAT_R32G32B32_SFLOAT, 0});
    location++;
    bindingIndex++;
    offset += sizeof(vsg::vec3);

    attributes.emplace_back(VkVertexInputAttributeDescription{
        bindingIndex, location, VK_FORMAT_R32G32B32_SFLOAT, 0});
    location++;
    bindingIndex++;
    offset += sizeof(vsg::vec3);

    attributes.emplace_back(VkVertexInputAttributeDescription{
        bindingIndex, location, VK_FORMAT_R32G32_SFLOAT, 0});
    location++;
    bindingIndex++;
    offset += sizeof(vsg::vec2);    

    /// Pipeline States
    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    // rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;
    auto depthStencilState = vsg::DepthStencilState::create();
    depthStencilState->depthTestEnable = VK_TRUE;
    auto graphicsPipelineStates = vsg::GraphicsPipelineStates{
        vertexInputState,
        inputAssemblyState,
        rasterizationState,
        vsg::TessellationState::create(4),
        vsg::ColorBlendState::create(),
        vsg::MultisampleState::create(),
        depthStencilState};

    /// Graphics Pipeline
    auto pipeline = vsg::GraphicsPipeline::create(
        pipelineLayout, shaderStages, graphicsPipelineStates);

    return vsg::BindGraphicsPipeline::create(pipeline);
}

// Generate a terrain quad patch with normals based on heightmap data
vsg::ref_ptr<vsg::VertexIndexDraw> generateTerrain(vsg::ref_ptr<vsg::ushortArray2D> ktxTexture, vsg::ref_ptr<vsg::Options> options)
{    

    const uint32_t patchSize{64};
    const float uvScale{1.0f};

    uint32_t dim;
    uint32_t scale;

    dim = ktxTexture->width();
    scale = dim / patchSize;

    const uint32_t vertexCount = patchSize * patchSize;

    auto position = vsg::vec3Array::create(vertexCount);
    auto normals = vsg::vec3Array::create(vertexCount);
    auto uv = vsg::vec2Array::create(vertexCount);

    const float wx = 1.0f;
    const float wy = 1.0f;

    // Generate a two-dimensional vertex patch
    for (auto x = 0; x < patchSize; x++)
    {
        for (auto y = 0; y < patchSize; y++)
        {
            uint32_t index = (x + y * patchSize);
            position->at(index).x = x * wx + wx / 2.0f - (float)patchSize * wx / 2.0f;
            position->at(index).y = y * wy + wy / 2.0f - (float)patchSize * wy / 2.0f;
            position->at(index).z = 0.0f;

            uv->at(index).x = (float)x / (patchSize - 1) * uvScale;
            uv->at(index).y = (float)y / (patchSize - 1) * uvScale;
        }
    }

    // Calculate normals from the height map using a sobel filter
    for (auto x = 0; x < patchSize; x++)
    {
        for (auto y = 0; y < patchSize; y++)
        {
            // We get
            float heights[3][3];
            for (auto sx = -1; sx <= 1; sx++)
            {
                for (auto sy = -1; sy <= 1; sy++)
                {
                    // Get height at sampled position from heightmap
                    vsg::ivec2 rpos = vsg::ivec2(x + sx, y + sy) * vsg::ivec2(scale, scale);
                    rpos.x = std::max(0, std::min(rpos.x, (int)dim - 1));
                    rpos.y = std::max(0, std::min(rpos.y, (int)dim - 1));
                    rpos.x /= scale;
                    rpos.y /= scale;

                    heights[sx + 1][sy + 1] = (ktxTexture->at(rpos.x, rpos.y) * scale) / 65535.0f;
                }
            }

            vsg::vec3 normal;
            // Gx sobel filter
            normal.x = heights[0][0] - heights[2][0] + 2.0f * heights[0][1] - 2.0f * heights[2][1] + heights[0][2] - heights[2][2];
            // Gy sobel filter
            normal.y = heights[0][0] + 2.0f * heights[1][0] + heights[2][0] - heights[0][2] - 2.0f * heights[1][2] - heights[2][2];
            // Calculate missing up component of the normal using the filtered x and y axis
            // The first value controls the bump strength
            normal.z = 0.25f * sqrt(1.0f - normal.x * normal.x - normal.z * normal.z);

            normals->at(x + y * patchSize) = vsg::normalize(normal * vsg::vec3(2.0f, 1.0f, 2.0f));
        }
    }

    // Generate indices
    const uint32_t w = (patchSize - 1);
    const uint32_t indexCount = w * w * 4;

    auto indices = vsg::uintArray::create(indexCount);

    for (auto x = 0; x < w; x++)
    {
        for (auto y = 0; y < w; y++)
        {
            uint32_t index = (x + y * w) * 4;
            indices->at(index) = (x + y * patchSize);
            indices->at(index + 1) = indices->at(index) + patchSize;
            indices->at(index + 2) = indices->at(index + 1) + 1;
            indices->at(index + 3) = indices->at(index) + 1;
        }
    }

    vsg::DataList vertexArrays{position, normals, uv};
    auto drawCommands = vsg::VertexIndexDraw::create();
    drawCommands->assignArrays(vertexArrays);
    drawCommands->assignIndices(indices);
    drawCommands->indexCount = indices->width();
    drawCommands->instanceCount = 1;
    return drawCommands;
}

auto createScene(
    vsg::Path layersPath, 
    vsg::Path heightMapPath,
    vsg::ref_ptr<vsg::Options> options)
{
    auto sceneGraph = vsg::Group::create();
    
    auto ktxLayers = vsg::read(layersPath, options).cast<vsg::Data>();
    /*ktxLayers->properties.imageViewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    ktxLayers->properties.format = VK_FORMAT_R8G8B8A8_UNORM;*/
    // ktxLayers->properties.imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    auto o = vsg::read(heightMapPath, options);
    auto ktxHeightMap = o.cast<vsg::ushortArray2D>();
    ktxHeightMap->properties.format = VK_FORMAT_R16_UNORM;    

    /// Graphics Pipeline State Group
    auto stateGroup = vsg::StateGroup::create();
    auto bindGraphicsPipeline = createBindGraphicsPipeline();
    stateGroup->add(bindGraphicsPipeline);

    auto ubo = UBOValue::create();
    ubo->value().displacementFactor = 20.0f;
    ubo->value().tessellationFactor = 0.75f;
    ubo->value().viewportDim = vsg::vec2{1024.0f, 768.0f};
    ubo->value().tessellatedEdgeSize = 20.0f;
    ubo->value().lightPos.set(0, 0, 300, 1);
    ubo->properties.dataVariance = vsg::DYNAMIC_DATA;

    auto buffer = vsg::DescriptorBuffer::create(ubo);

    auto heightMapSampler = vsg::Sampler::create();
    heightMapSampler->maxLod = ktxHeightMap->properties.maxNumMipmaps;
    heightMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    heightMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    heightMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    heightMapSampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    auto heightMap = vsg::DescriptorImage::create(heightMapSampler, ktxHeightMap, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto layerSampler = vsg::Sampler::create();
    layerSampler->maxLod = ktxLayers->properties.maxNumMipmaps;    
    layerSampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    auto layers = vsg::DescriptorImage::create(layerSampler, ktxLayers, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(bindGraphicsPipeline->pipeline->layout->setLayouts[0], vsg::Descriptors{buffer, heightMap, layers});
    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, bindGraphicsPipeline->pipeline->layout, 0, descriptorSet);

    stateGroup->add(bindDescriptorSet);

    auto drawCmd = generateTerrain(ktxHeightMap, options);
    stateGroup->addChild(drawCmd);
    
    sceneGraph->addChild(stateGroup);

    return std::make_tuple(sceneGraph, ubo);
}

class Frustum
{
public:
    enum side
    {
        LEFT = 0,
        RIGHT = 1,
        TOP = 2,
        BOTTOM = 3,
        BACK = 4,
        FRONT = 5
    };
    std::array<vsg::dvec4, 6> planes;

    void update(vsg::dmat4 matrix)
    {
        planes[LEFT].x = matrix[0].w + matrix[0].x;
        planes[LEFT].y = matrix[1].w + matrix[1].x;
        planes[LEFT].z = matrix[2].w + matrix[2].x;
        planes[LEFT].w = matrix[3].w + matrix[3].x;

        planes[RIGHT].x = matrix[0].w - matrix[0].x;
        planes[RIGHT].y = matrix[1].w - matrix[1].x;
        planes[RIGHT].z = matrix[2].w - matrix[2].x;
        planes[RIGHT].w = matrix[3].w - matrix[3].x;

        planes[TOP].x = matrix[0].w - matrix[0].y;
        planes[TOP].y = matrix[1].w - matrix[1].y;
        planes[TOP].z = matrix[2].w - matrix[2].y;
        planes[TOP].w = matrix[3].w - matrix[3].y;

        planes[BOTTOM].x = matrix[0].w + matrix[0].y;
        planes[BOTTOM].y = matrix[1].w + matrix[1].y;
        planes[BOTTOM].z = matrix[2].w + matrix[2].y;
        planes[BOTTOM].w = matrix[3].w + matrix[3].y;

        planes[BACK].x = matrix[0].w + matrix[0].z;
        planes[BACK].y = matrix[1].w + matrix[1].z;
        planes[BACK].z = matrix[2].w + matrix[2].z;
        planes[BACK].w = matrix[3].w + matrix[3].z;

        planes[FRONT].x = matrix[0].w - matrix[0].z;
        planes[FRONT].y = matrix[1].w - matrix[1].z;
        planes[FRONT].z = matrix[2].w - matrix[2].z;
        planes[FRONT].w = matrix[3].w - matrix[3].z;

        for (auto i = 0; i < planes.size(); i++)
        {
            auto length = sqrt(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
            planes[i] /= length;
        }
    }

    bool checkSphere(vsg::dvec3 pos, float radius)
    {
        for (auto i = 0; i < planes.size(); i++)
        {
            if ((planes[i].x * pos.x) + (planes[i].y * pos.y) + (planes[i].z * pos.z) + planes[i].w <= -radius)
            {
                return false;
            }
        }
        return true;
    }
};

class FrustumUpdater : public vsg::Inherit<vsg::Visitor, FrustumUpdater>
{
public:

    FrustumUpdater(vsg::ref_ptr<UBOValue> ubo, vsg::ref_ptr<vsg::Camera> camera) 
        : _ubo(ubo) 
        , _camera(camera)
    {
    }

    void apply(vsg::FrameEvent& frame) override {

        Frustum frustum;
        frustum.update(_camera->projectionMatrix->transform() * _camera->viewMatrix->transform());
        
        for (int i = 0; i < std::size(frustum.planes); i++) {
            _ubo->value().frustumPlanes[i] = vsg::vec4(frustum.planes[i]);
        }

        _ubo->dirty();
    }

private:
    vsg::ref_ptr<UBOValue> _ubo;
    vsg::ref_ptr<vsg::Camera> _camera;
};

int main(int argc, char** argv)
{
    try
    {
        vsg::CommandLine arguments(&argc, argv);

        // set up defaults and read command line arguments to override them
        auto options = vsg::Options::create();
        options->sharedObjects = vsg::SharedObjects::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        if (int log_level = 0; arguments.read("--log-level", log_level)) vsg::Logger::instance()->level = vsg::Logger::Level(log_level);

        vsg::Path layers = arguments.value(std::string("numbers.ktx"), {"--layers", "-l"});
        vsg::Path heights = arguments.value(std::string("heightmap.ktx"), {"--heightmap", "-h"});

        auto [sceneGraph, ubo] = createScene(layers, heights, options);

        /// Window
        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->debugLayer = arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        windowTraits->windowTitle = "terrain";
        windowTraits->samples = VK_SAMPLE_COUNT_4_BIT;

        auto features = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
        features->get().tessellationShader = VK_TRUE;

        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Unable to create window" << std::endl;
            return 1;
        }
        auto gpuNumber = arguments.value(-1, "--gpu");
        if (gpuNumber == -1)
        {
            (void)window->getOrCreatePhysicalDevice();
        }
        else
        {
            auto instance = window->getOrCreateInstance();
            (void)window->getOrCreateSurface(); // fulfill contract of getOrCreatePhysicalDevice();
            auto& physicalDevices = instance->getPhysicalDevices();
            window->setPhysicalDevice(physicalDevices[gpuNumber]);
        }

        /// Camera
        auto lookAt = vsg::LookAt::create(
            vsg::dvec3{150, 175, 200},
            vsg::dvec3{0, 0, 0},
            vsg::dvec3{0, 0, 1});
        auto perspective = vsg::Perspective::create();
        auto viewportState = vsg::ViewportState::create(window->extent2D());
        auto camera = vsg::Camera::create(perspective, lookAt, viewportState);

        /// Viewer and CommandGraph
        auto viewer = vsg::Viewer::create();
        viewer->addWindow(window);
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        viewer->addEventHandler(vsg::Trackball::create(camera));
        viewer->addEventHandler(FrustumUpdater::create(ubo, camera));

        auto commandGraph = vsg::CommandGraph::create(window);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        /// View and RenderGraph
        auto view = vsg::View::create(camera);
        auto renderGraph = vsg::RenderGraph::create(window, view);
        commandGraph->addChild(renderGraph);

        /// Add the scene to the View and compile
        view->addChild(sceneGraph);
        viewer->compile();

        while (viewer->advanceToNextFrame())
        {
            viewer->handleEvents();
            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();
        }
    }
    catch (const vsg::Exception& ve)
    {
        std::cerr << ve.message << std::endl;
        return 1;
    }
}
