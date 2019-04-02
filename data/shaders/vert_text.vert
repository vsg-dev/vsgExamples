#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;

// per instance
layout(location = 1) in vec3 instPostion;
layout(location = 2) in vec2 instOffset;
layout(location = 3) in float instLookupOffset;

layout(set = 0, binding = 0) uniform sampler2D glyphUVs;
layout(set = 0, binding = 1) uniform sampler2D glyphSizes;

layout(set = 1, binding = 3) uniform TextMetrics
{
    float height;
    float lineHeight;
} textMetrics;

layout(location = 0) out lowp vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    // lookup glyph data
    vec4 glyphUvRect = texture(glyphUVs, vec2(instLookupOffset, 0.5));
    vec4 glyphSize = texture(glyphSizes, vec2(instLookupOffset, 0.5)) * textMetrics.height;

    vec2 offset = (instOffset.xy * textMetrics.height) + glyphSize.xy;
	vec2 size = inPosition.xy * glyphSize.zw;

    vec2 vert = offset + size;
	vec3 point = instPostion + vec3(vert, 0.0);
    gl_Position = (pc.projection * pc.view * pc.model) * vec4(point, 1.0);
    
    fragTexCoord = (inPosition.xy * glyphUvRect.zw) + glyphUvRect.xy;
}
