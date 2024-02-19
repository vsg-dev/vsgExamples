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
    vec2(0.7143362959596435, 0.6342443385910527),
    vec2(-0.805348881112303, -0.5592663038183104),
    vec2(-0.6072798499728347, 0.7675410995073209),
    vec2(0.49942057483823643, -0.8580637300684422),
    vec2(0.018053933666046604, 0.010482905909003883),
    vec2(0.8185676408737856, -0.11336156565537749),
    vec2(-0.7231602025001969, 0.10147760824026557),
    vec2(-0.20681787816945832, -0.9632259483101246),
    vec2(0.05176039920239751, 0.9708536385933172),
    vec2(-0.17023781017825862, 0.5059535107076307),
    vec2(0.43110666769656364, -0.369249775076004),
    vec2(-0.2673067881819701, -0.4742794401187716),
    vec2(0.28883841787081194, 0.4524950513234204),
    vec2(0.12694902560130755, -0.5975128364544097),
    vec2(0.6031888909942549, 0.20989642059132863),
    vec2(-0.8604532188568536, 0.46058732108417244),
    vec2(-0.6177606157919132, -0.2821497065644401),
    vec2(0.8022616913515712, -0.4860280085709965),
    vec2(-0.5166505306246649, -0.7721370956769076),
    vec2(0.4883086046096338, 0.8613624856530216),
    vec2(0.1640568689584325, -0.9766485171283904),
    vec2(0.9637373946308719, 0.2601402938940584),
    vec2(-0.3171171407120015, 0.21724563488203644),
    vec2(0.31685391211395236, -0.016074311260097916),
    vec2(-0.3171695253245275, -0.10262952059558215),
    vec2(-0.9591701566847509, -0.27179607362749736),
    vec2(-0.32434456322559896, 0.8444263392562922),
    vec2(-0.521728005098972, 0.4512368990386342),
    vec2(0.07287913711592053, -0.28615027385157404),
    vec2(0.549962976568752, -0.08198195610807973),
    vec2(-0.9909680815749499, 0.03272054775428336),
    vec2(0.08843461245702418, 0.2595473369188091),
    vec2(0.5156127670269796, -0.6197781105237503),
    vec2(0.5240926711561459, 0.4468497480074937),
    vec2(0.39634463437531653, 0.655219176563734),
    vec2(0.09299551156856194, 0.6412470779389284),
    vec2(-0.20705524186345578, -0.7384529336739303),
    vec2(0.7878279255056497, 0.10679173712030011),
    vec2(-0.11475599326799543, 0.720496993670248),
    vec2(-0.5949494919824188, -0.5349758629065084),
    vec2(0.2202008919317203, 0.8274061824701594),
    vec2(-0.37253338385646945, 0.632935280511181),
    vec2(0.11746523372173562, -0.7900126849350121),
    vec2(0.6512844998007621, -0.273815026869956),
    vec2(0.3822339526326755, 0.22302834924229142),
    vec2(-0.0673641366406629, -0.581811741863646),
    vec2(-0.6997001169034057, 0.5884068817422222),
    vec2(-0.6403778144856442, -0.07177853031143179),
    vec2(0.7465008764005826, 0.3996145626312525),
    vec2(0.9839124543709388, 0.05799061002113045),
    vec2(-0.49620126212799914, 0.10798838658193986),
    vec2(0.937330574914645, -0.2598585335401833),
    vec2(-0.19782887296888754, -0.2729750222299262),
    vec2(0.2686541986861816, -0.2255921926560753),
    vec2(0.05824617309930314, 0.4480629872250429),
    vec2(-0.12775605805263238, 0.9346289382658581),
    vec2(-0.705913396585671, 0.343859725100361),
    vec2(-0.8318663382869211, -0.109853673830252),
    vec2(-0.3882099550885468, -0.3315451281460165),
    vec2(0.3094939253248168, -0.8461910932604467),
    vec2(0.34595049322453647, -0.535306277814495),
    vec2(-0.16889800608248826, 0.09427971140804951),
    vec2(0.7236079209683561, -0.6762131974011036),
    vec2(-0.8952524079003322, 0.26695407119959574),
};

const float PI = radians(180);

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

                    const float shadowSamples = 8;
                    const float radius = 0.05;

                    // hopefully relatively temporaly stable. world position would be better.
                    float diskRotation = mod((eyePos.x + eyePos.y + eyePos.z) * 100, 2) * PI;
                    mat2 diskRotationMatrix = mat2(cos(diskRotation), sin(diskRotation), -sin(diskRotation), cos(diskRotation));

                    float coverage = 0;
                    for (int i = 0; i < min(shadowSamples, POISSON_DISK_SAMPLE_COUNT); ++i)
                    {
                        vec2 rotatedDisk = diskRotationMatrix * POISSON_DISK[i];
                        sm_tc = sm_matrix * vec4(eyePos + radius * rotatedDisk.x * T + radius * rotatedDisk.y * B, 1.0);
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
