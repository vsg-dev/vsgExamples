#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

#pragma import_defines (VSG_TEXTURECOORD_0, VSG_TEXTURECOORD_1, VSG_TEXTURECOORD_2, VSG_TEXTURECOORD_3, VSG_POINT_SPRITE, VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_DETAIL_MAP, DEPTHPEELING_PASS, DEPTHPEELING_FIRSTPASS)

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1
#define PEEL_DESCRIPTOR_SET 2

#if defined(VSG_TEXTURECOORD_3)
    #define VSG_TEXCOORD_COUNT 4
#elif defined(VSG_TEXTURECOORD_2)
    #define VSG_TEXCOORD_COUNT 3
#elif defined(VSG_TEXTURECOORD_1)
    #define VSG_TEXCOORD_COUNT 2
#else
    #define VSG_TEXCOORD_COUNT 1
#endif

#ifdef VSG_DIFFUSE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D diffuseMap;
#endif

#ifdef VSG_DETAIL_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D detailMap;
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

layout(location = 2) in vec4 vertexColor;
layout(location = 4) in vec2 texCoord[VSG_TEXCOORD_COUNT];

#ifdef DEPTHPEELING_PASS
layout(input_attachment_index = 0, set = PEEL_DESCRIPTOR_SET, binding = 0) uniform subpassInput opaqueDepth;
layout(input_attachment_index = 1, set = PEEL_DESCRIPTOR_SET, binding = 1) uniform subpassInput prevPassDepth;
#endif

layout(location = 0) out vec4 outColor;

void main()
{
#ifdef DEPTHPEELING_PASS
    if (gl_FragCoord.z <= subpassLoad(opaqueDepth).r) 
        discard;

#ifndef DEPTHPEELING_FIRSTPASS
    if (gl_FragCoord.z >= subpassLoad(prevPassDepth).r) 
        discard;
#endif
#endif

#ifdef VSG_POINT_SPRITE
    const vec2 texCoordDiffuse = gl_PointCoord.xy;
#else
    const vec2 texCoordDiffuse = texCoord[texCoordIndices.diffuseMap].st;
#endif

    vec4 diffuseColor = vertexColor * material.diffuseColor;

#ifdef VSG_DIFFUSE_MAP
    #ifdef VSG_GREYSCALE_DIFFUSE_MAP
        float v = texture(diffuseMap, texCoordDiffuse);
        diffuseColor *= vec4(v, v, v, 1);
    #else
        diffuseColor *= texture(diffuseMap, texCoordDiffuse);
    #endif
#endif

#ifdef VSG_DETAIL_MAP
    vec4 detailColor = texture(detailMap, texCoord[texCoordIndices.detailMap].st);
    diffuseColor.rgb = mix(diffuseColor.rgb, detailColor.rgb, detailColor.a);
#endif


#ifdef DEPTHPEELING_PASS
    if (diffuseColor.a < (1.0 - material.alphaMaskCutoff) || diffuseColor.a >= material.alphaMaskCutoff) discard;
#else
    if (diffuseColor.a < material.alphaMaskCutoff) discard;
    diffuseColor.a = 1.0;
#endif

    outColor = vec4(diffuseColor.rgb, diffuseColor.a);
}
