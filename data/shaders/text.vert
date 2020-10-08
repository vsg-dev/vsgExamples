#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inOutlineColor;
layout(location = 3) in float inOutlineWidth;
layout(location = 4) in vec3 inTexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 outlineColor;
layout(location = 2) out float outlineWidth;
layout(location = 3) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = (pc.projection * pc.modelview) * vec4(inPosition, 1.0);
    gl_Position.z -= inTexCoord.z*0.001;
    fragColor = inColor;
    outlineColor = inOutlineColor;
    outlineWidth = inOutlineWidth;
    fragTexCoord = inTexCoord.xy;
}
