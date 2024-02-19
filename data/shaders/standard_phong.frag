#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_POINT_SPRITE, VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, SHADOWMAP_DEBUG)

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1

#ifdef VSG_DIFFUSE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D diffuseMap;
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

layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;


layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform texture2DArray shadowMaps;
layout(set = VIEW_DESCRIPTOR_SET, binding = 4) uniform sampler shadowMapShadowSampler;

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
#ifndef VSG_POINT_SPRITE
layout(location = 3) in vec2 texCoord0;
#endif
layout(location = 5) in vec3 viewDir;

layout(location = 0) out vec4 outColor;

const int POISSON_DISK_SAMPLE_COUNT = 64;
const vec2 POISSON_DISK[POISSON_DISK_SAMPLE_COUNT] = {
    vec2(0.111362, -0.945846),
    vec2(0.249528, -0.729546),
    vec2(-0.033228, -0.785056),
    vec2(-0.229432, -0.886982),
    vec2(0.089732, -0.608662),
    vec2(-0.194706, -0.684832),
    vec2(-0.054674, -0.422722),
    vec2(0.311898, -0.905452),
    vec2(0.168622, -0.251796),
    vec2(-0.258086, -0.425508),
    vec2(-0.228694, -0.142222),
    vec2(0.292138, -0.483846),
    vec2(0.419434, -0.658382),
    vec2(-0.476864, -0.812014),
    vec2(-0.373576, -0.62819),
    vec2(-0.451698, -0.435044),
    vec2(-0.23023, 0.15431),
    vec2(-0.378834, -0.249026),
    vec2(0.019092, -0.05238),
    vec2(-0.493658, 0.013474),
    vec2(0.292798, -0.06695),
    vec2(0.520752, -0.32847),
    vec2(-0.80834, -0.076574),
    vec2(-0.611162, 0.281098),
    vec2(-0.647738, -0.20885),
    vec2(-0.719894, 0.128536),
    vec2(0.589644, -0.787684),
    vec2(-0.716044, -0.393748),
    vec2(0.657816, -0.583412),
    vec2(-0.889678, 0.231794),
    vec2(-0.997064, 0.074024),
    vec2(-0.773342, 0.426748),
    vec2(-0.654816, -0.57601),
    vec2(-0.933314, -0.219452),
    vec2(0.843246, -0.386514),
    vec2(0.29256, 0.137446),
    vec2(0.477912, -0.09703),
    vec2(0.108196, 0.240964),
    vec2(0.48622, 0.108522),
    vec2(0.405996, 0.455534),
    vec2(0.030242, 0.4132),
    vec2(0.672428, 0.077154),
    vec2(0.653782, -0.151882),
    vec2(0.625518, 0.26701),
    vec2(-0.413014, 0.37324),
    vec2(-0.178842, 0.389552),
    vec2(0.97764, -0.006302),
    vec2(0.971214, -0.235178),
    vec2(-0.556602, 0.558602),
    vec2(-0.389692, 0.70583),
    vec2(0.861706, 0.367042),
    vec2(0.78719, 0.556584),
    vec2(-0.220418, 0.60428),
    vec2(-0.54086, 0.838524),
    vec2(-0.26583, 0.906462),
    vec2(0.210364, 0.47174),
    vec2(0.580556, 0.522612),
    vec2(0.48658, 0.772678),
    vec2(0.170294, 0.656776),
    vec2(0.67646, 0.731538),
    vec2(-0.037086, 0.647458),
    vec2(0.273456, 0.836342),
    vec2(-0.735696, 0.674614),
    vec2(-0.05145, 0.842418),
};

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    vec3 result;
#ifdef VSG_NORMAL_MAP
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normalMap, texCoord0).xyz * 2.0 - 1.0;

    //tangentNormal *= vec3(2,2,1);

    vec3 q1 = dFdx(eyePos);
    vec3 q2 = dFdy(eyePos);
    vec2 st1 = dFdx(texCoord0);
    vec2 st2 = dFdy(texCoord0);

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
    float brightnessCutoff = 0.001;

#ifdef VSG_POINT_SPRITE
    vec2 texCoord0 = gl_PointCoord.xy;
#endif

    vec4 diffuseColor = vertexColor * material.diffuseColor;
#ifdef VSG_DIFFUSE_MAP
    #ifdef VSG_GREYSCALE_DIFFUSE_MAP
        float v = texture(diffuseMap, texCoord0.st).s;
        diffuseColor *= vec4(v, v, v, 1.0);
    #else
        diffuseColor *= texture(diffuseMap, texCoord0.st);
    #endif
#endif

    vec4 ambientColor = diffuseColor * material.ambientColor * material.ambientColor.a;
    vec4 specularColor = material.specularColor;
    vec4 emissiveColor = material.emissiveColor;
    float shininess = material.shininess;
    float ambientOcclusion = 1.0;

    if (material.alphaMask == 1.0f)
    {
        if (diffuseColor.a < material.alphaMaskCutoff)
            discard;
    }

#ifdef VSG_EMISSIVE_MAP
    emissiveColor *= texture(emissiveMap, texCoord0.st);
#endif

#ifdef VSG_LIGHTMAP_MAP
    ambientOcclusion *= texture(aoMap, texCoord0.st).r;
#endif

#ifdef VSG_SPECULAR_MAP
    specularColor *= texture(specularMap, texCoord0.st);
#endif

    vec3 nd = getNormal();
    vec3 vd = normalize(viewDir);

    vec3 color = vec3(0.0, 0.0, 0.0);

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int index = 1;

    if (numAmbientLights>0)
    {
        // ambient lights
        for(int i = 0; i<numAmbientLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            color += (ambientColor.rgb * lightColor.rgb) * (lightColor.a);
        }
    }

    // index used to step through the shadowMaps array
    int shadowMapIndex = 0;

    if (numDirectionalLights>0)
    {
        vec3 q1 = dFdx(eyePos);
        vec3 q2 = dFdy(eyePos);
        vec2 st1 = dFdx(texCoord0);
        vec2 st2 = dFdy(texCoord0);

        vec3 N = normalize(normalDir);
        vec3 T = normalize(q1 * st2.t - q2 * st1.t);
        vec3 B = -normalize(cross(N, T));

        // directional lights
        for(int i = 0; i<numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            vec3 direction = -lightData.values[index++].xyz;
            vec4 shadowMapSettings = lightData.values[index++];

            float brightness = lightColor.a;

            // check shadow maps if required
            bool matched = false;
            while ((shadowMapSettings.r > 0.0 && brightness > brightnessCutoff) && !matched)
            {
                mat4 sm_matrix = mat4(lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++]);

                vec4 sm_tc = (sm_matrix) * vec4(eyePos, 1.0);

                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0 /* && sm_tc.z <= 1.0*/)
                {
                    matched = true;

                    const float shadowSamples = 5;
                    const float radius = 0.05;

                    float coverage = 0;
                    for (int i = 0; i < min(shadowSamples, POISSON_DISK_SAMPLE_COUNT); ++i)
                    {
                        sm_tc = sm_matrix * vec4(eyePos + radius * POISSON_DISK[i].x * T + radius * POISSON_DISK[i].y * B, 1.0);
                        coverage += texture(sampler2DArrayShadow(shadowMaps, shadowMapShadowSampler), vec4(sm_tc.st, shadowMapIndex, sm_tc.z)).r;
                    }

                    coverage /= min(shadowSamples, POISSON_DISK_SAMPLE_COUNT);

                    brightness *= (1.0-coverage);

#ifdef SHADOWMAP_DEBUG
                    if (shadowMapIndex==0) color = vec3(1.0, 0.0, 0.0);
                    else if (shadowMapIndex==1) color = vec3(0.0, 1.0, 0.0);
                    else if (shadowMapIndex==2) color = vec3(0.0, 0.0, 1.0);
                    else if (shadowMapIndex==3) color = vec3(1.0, 1.0, 0.0);
                    else if (shadowMapIndex==4) color = vec3(0.0, 1.0, 1.0);
                    else color = vec3(1.0, 1.0, 1.0);
#endif
                }

                ++shadowMapIndex;
                shadowMapSettings.r -= 1.0;
            }

            if (shadowMapSettings.r > 0.0)
            {
                // skip lightData and shadowMap entries for shadow maps that we haven't visited for this light
                // so subsequent light pointions are correct.
                index += 4 * int(shadowMapSettings.r);
                shadowMapIndex += int(shadowMapSettings.r);
            }

            // if light is too dim/shadowed to effect the rendering skip it
            if (brightness <= brightnessCutoff ) continue;

            float unclamped_LdotN = dot(direction, nd);

            float diff = max(unclamped_LdotN, 0.0);
            color.rgb += (diffuseColor.rgb * lightColor.rgb) * (diff * brightness);

            if (shininess > 0.0 && diff > 0.0)
            {
                vec3 halfDir = normalize(direction + vd);
                color.rgb += specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), shininess) * brightness);
            }
        }
    }

    if (numPointLights>0)
    {
        // point light
        for(int i = 0; i<numPointLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            vec3 position = lightData.values[index++].xyz;

            float brightness = lightColor.a;

            // if light is too dim/shadowed to effect the rendering skip it
            if (brightness <= brightnessCutoff ) continue;

            vec3 delta = position - eyePos;
            float distance2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            vec3 direction = delta / sqrt(distance2);
            float scale = brightness / distance2;

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
        // spot light
        for(int i = 0; i<numSpotLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            vec4 position_cosInnerAngle = lightData.values[index++];
            vec4 lightDirection_cosOuterAngle = lightData.values[index++];

            float brightness = lightColor.a;

            // if light is too dim/shadowed to effect the rendering skip it
            if (brightness <= brightnessCutoff ) continue;

            vec3 delta = position_cosInnerAngle.xyz - eyePos;
            float distance2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            vec3 direction = delta / sqrt(distance2);

            float dot_lightdirection = dot(lightDirection_cosOuterAngle.xyz, -direction);
            float scale = (brightness  * smoothstep(lightDirection_cosOuterAngle.w, position_cosInnerAngle.w, dot_lightdirection)) / distance2;

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
