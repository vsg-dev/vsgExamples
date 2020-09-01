#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(constant_id = 0) const uint numBaseTextures = 1;

layout(set = 0, binding = 0) uniform sampler2D heightField[numBaseTextures];

layout(set = 1, binding = 0) uniform TileSettings {
    uint tileIndex;
} tileSettings;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    float height = texture(heightField[tileSettings.tileIndex], inTexCoord).x * 0.1;
    vec4 position = vec4(inPosition.x, inPosition.y, inPosition.z + height, 1.0);

    gl_Position = (pc.projection * pc.modelview) * position;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
