#version 450

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

//#define GREYCALE

void main()
{
#if GREYSCALE
#if 0
    float lod = textureQueryLod(texSampler, fragTexCoord).x;
    float start_cutoff = 2.0;
    float end_cutoff = 8.0;
    float alpha_multiplier = mix(1.0, 0.0, (lod-start_cutoff)/(end_cutoff-start_cutoff));
#else
    float alpha_multiplier = 1.0;
#endif
    outColor = vec4(fragColor.rgb, fragColor.a * texture(texSampler, fragTexCoord).r * alpha_multiplier);
#else

    float distance_from_edge = (texture(texSampler, fragTexCoord).r - 0.5);
    float alpha_multiplier = 0.0;
#if 1
    float blend_distance = -0.05;
    if (distance_from_edge >= 0.0) alpha_multiplier = 1.0;
    else if (distance_from_edge >= blend_distance)
    {
        float boundary_ratio = (distance_from_edge) / blend_distance;
        alpha_multiplier = smoothstep(1.0, 0.0, boundary_ratio);
    }
#else
    if (distance_from_edge >= 0.0) alpha_multiplier = 1.0;
#endif

    outColor = vec4(fragColor.rgb, fragColor.a * alpha_multiplier );
#endif

    //outColor = fragColor;
    if (outColor.a == 0.0) discard;
}
