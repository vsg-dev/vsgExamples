#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// specialization constants
layout(constant_id = 0) const uint numGlyphMetrics = 1;

struct GlyphMetrics
{
    vec4 uvrect; // min x/y, max x/y
    float width;
    float height;
    float horiBearingX;
    float horiBearingY;
    float horiAdvance;
    float vertBearingX;
    float vertBearingY;
    float vertAdvance;
};

layout(set = 0, binding = 1) uniform sampler2D glyphMetricsSampler;

layout(std140, set = 1, binding = 0) uniform TextLayout {
    vec4 position;
    vec4 horizontal;
    vec4 vertical;
    vec4 color;
    vec4 outlineColor;
    float outlineWidth;
} textLayout;

layout(set = 1, binding = 1) uniform TextIndices {
    uint glyph_index[1];
} text;


layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 outlineColor;
layout(location = 2) out float outlineWidth;
layout(location = 3) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {

    vec3 pos = inPosition + textLayout.horizontal.xyz * (text.glyph_index[gl_InstanceIndex]*0.1);
    gl_Position = (pc.projection * pc.modelview) * vec4(pos, 1.0);

    fragColor = textLayout.color;
    outlineColor = textLayout.outlineColor;
    outlineWidth = textLayout.outlineWidth.x;

    fragTexCoord = inPosition.xy;
}
