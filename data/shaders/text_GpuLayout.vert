#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

// specialization constants
layout(constant_id = 0) const uint numTextIndices = 1;

#define GLYPH_DIMENSIONS 0
#define GLYPH_BEARINGS 1
#define GLYPH_UVRECT 2

layout(set = 0, binding = 1) uniform sampler2D glyphMetricsSampler;

layout(set = 1, binding = 0) uniform TextLayout {
    vec4 position;
    vec4 horizontal;
    vec4 vertical;
    vec4 color;
    vec4 outlineColor;
    float outlineWidth;
} textLayout;

layout(set = 1, binding = 1) uniform TextIndices {
    uvec4 glyph_index[numTextIndices];
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
            horiAdvance += texture(glyphMetricsSampler, vec2(GLYPH_DIMENSIONS, glyph_index))[2];
        }
    }
    vec3 cursor = textLayout.position.xyz + textLayout.horizontal.xyz * horiAdvance + textLayout.vertical.xyz * vertAdvance;

    // compute the position of vertex
    uint glyph_index = text.glyph_index[gl_InstanceIndex / 4][gl_InstanceIndex % 4];

    vec4 dimensions = texture(glyphMetricsSampler, vec2(GLYPH_DIMENSIONS, glyph_index));
    vec4 bearings = texture(glyphMetricsSampler, vec2(GLYPH_BEARINGS, glyph_index));
    vec4 uv_rec = texture(glyphMetricsSampler, vec2(GLYPH_UVRECT, glyph_index));

    vec3 pos = cursor + textLayout.horizontal.xyz * (bearings.x + inPosition.x * dimensions.x) + textLayout.vertical.xyz * (bearings.y + (inPosition.y-1.0) * dimensions.y);

    gl_Position = (pc.projection * pc.modelview) * vec4(pos, 1.0);
    gl_Position.z -= inPosition.z*0.001;

    fragColor = textLayout.color;
    outlineColor = textLayout.outlineColor;
    outlineWidth = textLayout.outlineWidth;

    fragTexCoord = vec2(mix(uv_rec[0], uv_rec[2], inPosition.x), mix(uv_rec[1], uv_rec[3], inPosition.y));
}
