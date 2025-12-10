#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 normalDir;
layout(location = 1) in vec3 eyePos;
layout(location = 2) in vec3 viewDir;
layout(set = 0, binding = 0) uniform SlcColors
{
    vec4 values[3];
} slcColors;
layout(set = 1, binding = 0) uniform LightData
{
    vec4 values[256]; // 256???
} lightData;
layout(set = 2, binding = 0) uniform MaterialData
{
    vec4 ambientColor;
    vec4 diffuseColor;
    vec4 specularColor;
    vec4 emissiveColor;
    float shininess;
    float alphaMask;
    float alphaMaskCutoff;
} material;
layout(set = 3, binding = 0) uniform SlcIndex
{
    uint value;
} slcIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    float intensityMinimum = 0.001;
    vec3 nd = normalize(normalDir);
    vec3 vd = normalize(viewDir);

    vec3 color = vec3(0.0, 0.0, 0.0);
    vec3 diffuse = material.diffuseColor.rgb;
    if (slcIndex.value == 1 || slcIndex.value == 2) diffuse = slcColors.values[slcIndex.value].rgb;

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int lightDataIndex = 1;


    if (numAmbientLights > 0)
    {
        // ambient lights
        for(int i = 0; i < numAmbientLights; ++i)
        {
            vec4 lightColor = lightData.values[lightDataIndex++];
            color += (material.ambientColor.rgb * lightColor.rgb) * (lightColor.a);
        }
    }

    if (numDirectionalLights > 0)
    {
        for(int i = 0; i < numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[lightDataIndex++];
            vec3 direction = -lightData.values[lightDataIndex++].xyz;

            float intensity = lightColor.a;

            float unclamped_LdotN = dot(direction, nd);
            float diff = max(unclamped_LdotN, 0.0);
            intensity *= diff;

            color.rgb += (diffuse.rgb * lightColor.rgb) * (intensity);

            if (material.shininess > 0.0 && diff > 0.0)
            {
                vec3 halfDir = normalize(direction + vd);
                color.rgb += material.specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), material.shininess) * intensity);
            }
            lightDataIndex++;
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

            color.rgb += (diffuse.rgb * lightColor.rgb) * diff;
            if (material.shininess > 0.0 && diff > 0.0)
            {
                vec3 halfDir = normalize(direction + vd);
                color.rgb += material.specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), material.shininess) * scale);
            }
        }
    }

    if (numSpotLights>0)
    {
        for(int i = 0; i < numSpotLights; ++i)
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

            // if light is too shadowed to effect the rendering skip it
            if (intensity < intensityMinimum ) continue;

            float scale = (intensity * smoothstep(lightDirection_cosOuterAngle.w, position_cosInnerAngle.w, dot_lightdirection)) / distance2;

            float unclamped_LdotN = dot(direction, nd);

            float diff = scale * max(unclamped_LdotN, 0.0);
            color.rgb += (diffuse.rgb * lightColor.rgb) * diff;
            if (material.shininess > 0.0 && diff > 0.0)
            {
                vec3 halfDir = normalize(direction + vd);
                color.rgb += material.specularColor.rgb * (pow(max(dot(halfDir, nd), 0.0), material.shininess) * scale);
            }
            lightDataIndex++;
        }
    }

    outColor = vec4(color, material.diffuseColor.a);
}
