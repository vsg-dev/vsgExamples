#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 cameraPos;
layout(location = 1) out vec4 vertexPos;
layout(location = 2) out mat4 texgen;

out gl_PerVertex{
    vec4 gl_Position;
};

void main() {
    gl_Position = (pc.projection * pc.modelview) * vec4(inPosition, 1.0);
    cameraPos = inverse(pc.modelview) * vec4(0,0,0,1);
    vertexPos = vec4(inPosition, 1.0);
    texgen = mat4(1.0);
}
