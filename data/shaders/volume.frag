#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler3D volume;

layout(location = 0) in vec4 cameraPos;
layout(location = 1) in vec4 vertexPos;
layout(location = 2) in mat4 texgen;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 t0 = vertexPos;
    vec4 te = cameraPos;
    if( te.x>=0.0 && te.x<=1.0 &&
        te.y>=0.0 && te.y<=1.0 &&
        te.z>=0.0 && te.z<=1.0
        )
    { } else {
        if (te.x<0.0){
            float r = -te.x / (t0.x-te.x);
            te = te + (t0-te)*r;
        }
        if (te.x>1.0){
            float r = (1.0-te.x) / (t0.x-te.x);
            te = te + (t0-te)*r;
        }
        if (te.y<0.0){
            float r = -te.y / (t0.y-te.y);
            te = te + (t0-te)*r;
        }
        if (te.y>1.0){
            float r = (1.0-te.y) / (t0.y-te.y);
            te = te + (t0-te)*r;
        }
        if (te.z<0.0){
            float r = -te.z / (t0.z-te.z);
            te = te + (t0-te)*r;
        }
        if (te.z>1.0){
            float r = (1.0-te.z) / (t0.z-te.z);
            te = te + (t0-te)*r;
        }
    }
    t0 = t0 * texgen;
    te = te * texgen;

    const float min_iteratrions = 2.0;
    const float max_iteratrions = 2048.0;

    float TransparencyValue = 0.2;
    float AlphaFuncValue = 0.1;
    float SampleDensityValue = 0.005; // 0.5 / texture_sample_count

    float num_iterations = ceil(length((te-t0).xyz)/SampleDensityValue);
    if (num_iterations<min_iteratrions) num_iterations = min_iteratrions;
    else if (num_iterations>max_iteratrions) num_iterations = max_iteratrions;

    vec3 deltaTexCoord=(te-t0).xyz/(num_iterations-1.0);
    vec3 texcoord = t0.xyz;

    vec4 fragColor = vec4(0.0, 0.0, 0.0, 0.0);
    while(num_iterations>0.0)
    {
        float alpha = texture(volume, texcoord).r;
        vec4 color = vec4(alpha, alpha, alpha, alpha * TransparencyValue);
        float r = color.a;
        if (r > AlphaFuncValue)
        {
            fragColor.rgb = mix(fragColor.rgb, color.rgb, r);
            fragColor.a += r;
        }

        if (color.a > fragColor.a)
        {
            fragColor = color;
        }

        texcoord += deltaTexCoord;
        --num_iterations;
    }
    if (fragColor.a>1.0) fragColor.a = 1.0;
    if (fragColor.a<AlphaFuncValue) discard;
    outColor = fragColor;
}
