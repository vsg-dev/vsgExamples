#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants
{
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 vsg_Vertex;
layout(location = 1) in vec3 vsg_Normal;

layout(location = 0) out vec3 normalDir;
layout(location = 1) out vec3 eyePos;
layout(location = 2) out vec3 viewDir;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    vec4 tVertex = vec4(vsg_Vertex, 1.0);
    vec4 tNormal = vec4(vsg_Normal, 0.0);
    gl_Position = (pc.projection * pc.modelview) * tVertex;
    eyePos = (pc.modelview * tVertex).xyz;
    viewDir = - (pc.modelview * tVertex).xyz;
    normalDir = (pc.modelview * tNormal).xyz;
}
