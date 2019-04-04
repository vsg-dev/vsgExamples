#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 2) uniform sampler2D glyphAtlas;

layout(location = 0) in lowp vec2 fragTexCoord;

layout(location = 0) out lowp vec4 outColor;

void main() {
    outColor = texture(glyphAtlas, fragTexCoord);
}
