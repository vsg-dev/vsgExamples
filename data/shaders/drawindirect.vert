#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;

layout(location = 0) in vec4 vsg_Vertex;

layout(location = 0) out vec4 vertexColor;

void main()
{
    vec4 vertex = vec4(vsg_Vertex.xyz, 1.0);
    gl_Position = (pc.projection * pc.modelView) * vertex;
    vec3 eyePos = vec4(pc.modelView * vertex).xyz;

    float distance = -eyePos.z;

    vertexColor = vec4(1.0, 1.0, 1.0, 1.0);
}
