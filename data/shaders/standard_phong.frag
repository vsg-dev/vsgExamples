#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_TEXTURECOORD_0, VSG_TEXTURECOORD_1, VSG_TEXTURECOORD_2, VSG_TEXTURECOORD_3, VSG_POINT_SPRITE, VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_DETAIL_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_SHADOWS_PCSS, VSG_SHADOWS_SOFT, VSG_SHADOWS_HARD, SHADOWMAP_DEBUG, VSG_ALPHA_TEST)

// define by default for backwards compatibility
#define VSG_SHADOWS_HARD

#if defined(VSG_TEXTURECOORD_3)
    #define VSG_TEXCOORD_COUNT 4
#elif defined(VSG_TEXTURECOORD_2)
    #define VSG_TEXCOORD_COUNT 3
#elif defined(VSG_TEXTURECOORD_1)
    #define VSG_TEXCOORD_COUNT 2
#else
    #define VSG_TEXCOORD_COUNT 1
#endif

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1

const float PI = radians(180);

#ifdef VSG_DIFFUSE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D diffuseMap;
#endif

#ifdef VSG_DETAIL_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D detailMap;
#endif

#ifdef VSG_NORMAL_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D normalMap;
#endif

#ifdef VSG_LIGHTMAP_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D aoMap;
#endif

#ifdef VSG_EMISSIVE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 4) uniform sampler2D emissiveMap;
#endif

#ifdef VSG_SPECULAR_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D specularMap;
#endif

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 10) uniform MaterialData
{
    vec4 ambientColor;
    vec4 diffuseColor;
    vec4 specularColor;
    vec4 emissiveColor;
    float shininess;
    float alphaMask;
    float alphaMaskCutoff;
} material;

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 11) uniform TexCoordIndices
{
    // indices into texCoord[] array for each texture type
    int diffuseMap;
    int detailMap;
    int normalMap;
    int aoMap;
    int emissiveMap;
    int specularMap;
    int mrMap;
} texCoordIndices;

layout(constant_id = 3) const int lightDataSize = 256;
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[lightDataSize];
} lightData;

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec3 viewDir;
layout(location = 4) in vec2 texCoord[VSG_TEXCOORD_COUNT];

layout(location = 0) out vec4 outColor;

// include the calculateShadowCoverageForDirectionalLight(..) implementation
#include "shadows.glsl"

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    vec3 result;
#ifdef VSG_NORMAL_MAP
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normalMap, texCoord[texCoordIndices.normalMap]).xyz * 2.0 - 1.0;

    //tangentNormal *= vec3(2,2,1);

    vec3 q1 = dFdx(eyePos);
    vec3 q2 = dFdy(eyePos);
    vec2 st1 = dFdx(texCoord[texCoordIndices.normalMap]);
    vec2 st2 = dFdy(texCoord[texCoordIndices.normalMap]);

    vec3 N = normalize(normalDir);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    result = normalize(TBN * tangentNormal);
#else
    result = normalize(normalDir);
#endif
#ifdef VSG_TWO_SIDED_LIGHTING
    if (!gl_FrontFacing)
        result = -result;
#endif
    return result;
}

vec3 computeLighting(vec3 ambientColor, vec3 diffuseColor, vec3 specularColor, vec3 emissiveColor, float shininess, float ambientOcclusion, vec3 ld, vec3 nd, vec3 vd)
{
    vec3 color = vec3(0.0);
    color.rgb += ambientColor;

    float diff = max(dot(ld, nd), 0.0);
    color.rgb += diffuseColor * diff;

    if (diff > 0.0)
    {
        vec3 halfDir = normalize(ld + vd);
        color.rgb += specularColor * pow(max(dot(halfDir, nd), 0.0), shininess);
    }

    vec3 result = color + emissiveColor;
    result *= ambientOcclusion;

    return result;
}


void main()
{
    float intensityMinimum = 0.001;

#ifdef VSG_POINT_SPRITE
    const vec2 texCoordDiffuse = gl_PointCoord.xy;
#else
    const vec2 texCoordDiffuse = texCoord[texCoordIndices.diffuseMap].st;
#endif

    vec4 diffuseColor = vertexColor * material.diffuseColor;
#ifdef VSG_DIFFUSE_MAP
    #ifdef VSG_GREYSCALE_DIFFUSE_MAP
        float v = texture(diffuseMap, texCoordDiffuse).s;
        diffuseColor *= vec4(v, v, v, 1.0);
    #else
        diffuseColor *= texture(diffuseMap, texCoordDiffuse);
    #endif
#endif

#ifdef VSG_DETAIL_MAP
    vec4 detailColor = texture(detailMap, texCoord[texCoordIndices.detailMap].st);
    diffuseColor.rgb = mix(diffuseColor.rgb, detailColor.rgb, detailColor.a);
#endif

    vec4 ambientColor = diffuseColor * material.ambientColor * material.ambientColor.a;
    vec4 specularColor = material.specularColor;
    vec4 emissiveColor = material.emissiveColor;
    float shininess = material.shininess;
    float ambientOcclusion = 1.0;

#ifdef VSG_ALPHA_TEST
    if (material.alphaMask == 1.0f && diffuseColor.a < material.alphaMaskCutoff) discard;
#endif

#ifdef VSG_EMISSIVE_MAP
    emissiveColor *= texture(emissiveMap, texCoord[texCoordIndices.emissiveMap].st);
#endif

#ifdef VSG_LIGHTMAP_MAP
    ambientOcclusion *= texture(aoMap, texCoord[texCoordIndices.aoMap].st).r;
#endif

#ifdef VSG_SPECULAR_MAP
    specularColor *= texture(specularMap, texCoord[texCoordIndices.specularMap].st);
#endif

    vec3 nd = getNormal();
    vec3 vd = normalize(viewDir);

    vec3 color = vec3(0.0, 0.0, 0.0);

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int lightDataIndex = 1;

    if (numAmbientLights>0)
    {
        // ambient lights
        for(int i = 0; i<numAmbientLights; ++i)
        {
            vec4 lightColor = lightData.values[lightDataIndex++];
            color += (ambientColor.rgb * lightColor.rgb) * (lightColor.a);
        }
    }

    // index used to step through the shadowMaps array
    int shadowMapIndex = 0;

    if (numDirectionalLights>0)
    {
        vec3 q1 = dFdx(eyePos);
        vec3 q2 = dFdy(eyePos);
        vec2 st1 = dFdx(texCoord[0]);
        vec2 st2 = dFdy(texCoord[0]);

        vec3 N = normalize(normalDir);
        vec3 T = normalize(q1 * st2.t - q2 * st1.t);
        vec3 B = -normalize(cross(N, T));

        // directional lights
        for(int i = 0; i<numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[lightDataIndex++];
            vec3 direction = -lightData.values[lightDataIndex++].xyz;

            float intensity = lightColor.a;

            float unclamped_LdotN = dot(direction, nd);

            float diff = max(unclamped_LdotN, 0.0);
            intensity *= diff;

            int shadowMapCount = int(lightData.values[lightDataIndex].r);
            if (shadowMapCount > 0)
            {
                if (intensity > intensityMinimum)
                    intensity *= (1.0-calculateShadowCoverageForDirectionalLight(lightDataIndex, shadowMapIndex, T, B, color));

                lightDataIndex += 1 + 8 * shadowMapCount;
                shadowMapIndex += shadowMapCount;
            }
            else
                lightDataIndex++;

            // if light is too shadowed to effect the rendering skip it
            if (intensity < intensityMinimum ) continue;

            color.rgb += (diffuseColor.rgb * lightColor.rgb) * (intensity);

            if (shininess > 0.0 && diff > 0.0)
            {
                vec3 halfDir = normalize(direction + vd);
                color.rgb += specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), shininess) * intensity);
            }
        }
    }

    if (numPointLights>0)
    {
        // point light
        for(int i = 0; i<numPointLights; ++i)
        {
            vec4 lightColor = lightData.values[lightDataIndex++];
            vec3 position = lightData.values[lightDataIndex++].xyz;

            float intensity = lightColor.a;

            // if light is too dim/shadowed to effect the rendering skip it
            if (intensity < intensityMinimum ) continue;

            vec3 delta = position - eyePos;
            float distance2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            vec3 direction = delta / sqrt(distance2);
            float scale = intensity / distance2;

            float unclamped_LdotN = dot(direction, nd);

            float diff = scale * max(unclamped_LdotN, 0.0);

            color.rgb += (diffuseColor.rgb * lightColor.rgb) * diff;
            if (shininess > 0.0 && diff > 0.0)
            {
                vec3 halfDir = normalize(direction + vd);
                color.rgb += specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), shininess) * scale);
            }
        }
    }

    if (numSpotLights>0)
    {
        vec3 q1 = dFdx(eyePos);
        vec3 q2 = dFdy(eyePos);
        vec2 st1 = dFdx(texCoord[0]);
        vec2 st2 = dFdy(texCoord[0]);

        vec3 N = normalize(normalDir);
        vec3 T = normalize(q1 * st2.t - q2 * st1.t);
        vec3 B = -normalize(cross(N, T));

        // spot light
        for(int i = 0; i<numSpotLights; ++i)
        {
            vec4 lightColor = lightData.values[lightDataIndex++];
            vec4 position_cosInnerAngle = lightData.values[lightDataIndex++];
            vec4 lightDirection_cosOuterAngle = lightData.values[lightDataIndex++];

            float intensity = lightColor.a;

            vec3 delta = position_cosInnerAngle.xyz - eyePos;
            float distance2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            float dist = sqrt(distance2);

            vec3 direction = delta / dist;

            float dot_lightdirection = dot(lightDirection_cosOuterAngle.xyz, -direction);

            int shadowMapCount = int(lightData.values[lightDataIndex].r);
            if (shadowMapCount > 0)
            {
                if (lightDirection_cosOuterAngle.w < dot_lightdirection)
                    intensity *= (1.0-calculateShadowCoverageForSpotLight(lightDataIndex, shadowMapIndex, T, B, dist, color));

                lightDataIndex += 1 + 8 * shadowMapCount;
                shadowMapIndex += shadowMapCount;
            }
            else
                lightDataIndex++;

            // if light is too shadowed to effect the rendering skip it
            if (intensity < intensityMinimum ) continue;

            float scale = (intensity * smoothstep(lightDirection_cosOuterAngle.w, position_cosInnerAngle.w, dot_lightdirection)) / distance2;

            float unclamped_LdotN = dot(direction, nd);

            float diff = scale * max(unclamped_LdotN, 0.0);
            color.rgb += (diffuseColor.rgb * lightColor.rgb) * diff;
            if (shininess > 0.0 && diff > 0.0)
            {
                vec3 halfDir = normalize(direction + vd);
                color.rgb += specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), shininess) * scale);
            }
        }
    }

    outColor.rgb = (color * ambientOcclusion) + emissiveColor.rgb;
    outColor.a = diffuseColor.a;
}
