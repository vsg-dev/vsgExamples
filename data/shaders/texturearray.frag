#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 0) const uint numBaseTextures = 1;

layout(binding = 0) uniform sampler2D baseTextureSampler[numBaseTextures];

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(baseTextureSampler[0], fragTexCoord);
}
