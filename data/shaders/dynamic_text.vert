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

#define MIN_TX 0
#define MIN_TY 1
#define MAX_TX 2
#define MAX_TY 3
#define WIDTH 4
#define HEIGHT 5
#define HORI_BEARING_X 6
#define HORI_BEARING_Y 7
#define HORI_ADVANCE 8
#define VERT_BEARING_X 9
#define VERT_BEARING_Y 10
#define VERT_ADVANCE 11

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
    uvec4 glyph_index[8];
} text;


layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 outlineColor;
layout(location = 2) out float outlineWidth;
layout(location = 3) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    // compute the position of the glyph
    float horiAdvance = 0.0;
    float vertAdvance = 0.0;
    for(uint i=0; i<gl_InstanceIndex; ++i)
    {
        uint glyph_index = text.glyph_index[i / 4][i % 4];
        if (glyph_index==0)
        {
            // treat as a newlline
            vertAdvance -= 1.0;
            horiAdvance = 0.0;
        }
        else
        {
            horiAdvance += texture(glyphMetricsSampler, vec2(HORI_ADVANCE, glyph_index)).x;
        }
    }
    vec3 cursor = textLayout.position.xyz + textLayout.horizontal.xyz * horiAdvance + textLayout.vertical.xyz * vertAdvance;

    // compute the position of vertex
    uint glyph_index = text.glyph_index[gl_InstanceIndex / 4][gl_InstanceIndex % 4];

    float min_tx = texture(glyphMetricsSampler, vec2(MIN_TX, glyph_index)).x;
    float min_ty = texture(glyphMetricsSampler, vec2(MIN_TY, glyph_index)).x;
    float max_tx = texture(glyphMetricsSampler, vec2(MAX_TX, glyph_index)).x;
    float max_ty = texture(glyphMetricsSampler, vec2(MAX_TY, glyph_index)).x;

    float width = texture(glyphMetricsSampler, vec2(WIDTH, glyph_index)).x;
    float height = texture(glyphMetricsSampler, vec2(HEIGHT, glyph_index)).x;
    float horiBearingX = texture(glyphMetricsSampler, vec2(HORI_BEARING_X, glyph_index)).x;
    float horiBearingY = texture(glyphMetricsSampler, vec2(HORI_BEARING_Y, glyph_index)).x;

    vec3 pos = cursor + textLayout.horizontal.xyz * (horiBearingX + inPosition.x * width) + textLayout.vertical.xyz * (horiBearingY - height + inPosition.y * height);

    gl_Position = (pc.projection * pc.modelview) * vec4(pos, 1.0);

    fragColor = textLayout.color;
    outlineColor = textLayout.outlineColor;
    outlineWidth = textLayout.outlineWidth;

    fragTexCoord = vec2(mix(min_tx, max_tx, inPosition.x), mix(min_ty, max_ty, inPosition.y));
}
