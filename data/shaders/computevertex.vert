#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;

layout(location = 0) in vec4 vsg_Vertex;

out gl_PerVertex{
    vec4 gl_Position;
    float gl_PointSize;
};

void main()
{
    vec4 vertex = vec4(vsg_Vertex.xyz, 1.0);
    gl_Position = (pc.projection * pc.modelView) * vertex;
    vec3 eyePos = vec4(pc.modelView * vertex).xyz;
    gl_PointSize = 50.0 / abs(eyePos.z);
}
