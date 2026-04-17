#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;

void main()
{
    // 0---^-----------2
    // | S | S |     /
    // <---.---|---/---> x+
    // | S | S | /
    // |-------/
    // |   | /
    // |   /
    // | / |
    // 1   V
    //     y+

    // gl_VertexIndex:
    //  0 = 0.0, 0.0, 0.0, 1.0
    //  1 = 0.0, 4.0, 0.0, 1.0
    //  2 = 4.0, 0.0, 0.0, 1.0
    vec4 pos = vec4((float((gl_VertexIndex >> 1U) & 1U)) * 4.0, (float(gl_VertexIndex & 1U)) * 4.0, 0, 1.0);
    // screen space for x/y from -1 to 1 -> so offset coordinates by -1
    pos.xy -= vec2(1.0, 1.0);
    
    gl_Position = pos;
}
