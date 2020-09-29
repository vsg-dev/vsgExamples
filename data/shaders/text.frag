#version 450

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
#if 1
    float lod = textureQueryLod(texSampler, fragTexCoord).x;
    float start_cutoff = 2.0;
    float end_cutoff = 8.0;
    float alpha_multiplier = mix(1.0, 0.0, (lod-start_cutoff)/(end_cutoff-start_cutoff));
#else
    float alpha_multiplier = 1.0;
#endif
    outColor = vec4(fragColor.rgb, fragColor.a * texture(texSampler, fragTexCoord).r * alpha_multiplier);
    //outColor = fragColor;
    if (outColor.a == 0.0) discard;
}
