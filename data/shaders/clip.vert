#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(set = 1, binding = 0) uniform ClipSettings {
    vec4 sphere; // x,y,z, radius
} clipSettings;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_ClipDistance[1];
    //float gl_CullDistance[1];
};

void main() {
    gl_Position = (pc.projection * pc.modelview) * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;

    vec4 eye_Position = pc.modelview * vec4(inPosition, 1.0);

    gl_ClipDistance[0] = (length(eye_Position.xyz - clipSettings.sphere.xyz) - clipSettings.sphere.w);
    // gl_CullDistance[0] = (length(inPosition - clipSettings.sphere.xyz) - 3.0);
}
