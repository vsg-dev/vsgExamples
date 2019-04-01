#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;

// per instance
layout(location = 1) in vec2 instOffset;
layout(location = 2) in float instLookupOffset;

layout(binding = 1) uniform sampler2D glyphUVs;
layout(binding = 2) uniform sampler2D glyphSizes;

layout(binding = 3) uniform TextMetrics
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
	vec4 glyphRect = texture(glyphUVs, vec2(instLookupOffset, 0.5));
	vec4 glyphSize = texture(glyphSizes, vec2(instLookupOffset, 0.5)) * textMetrics.height;

	vec2 scaledOffset = (instOffset * textMetrics.height) + glyphSize.zw;

	vec2 vert = (inPosition.xy * glyphSize.xy) + scaledOffset;
    gl_Position = (pc.projection * pc.view * pc.model) * vec4(vert, 0.0, 1.0);
	
	fragTexCoord = (inPosition.xy * glyphRect.zw) + glyphRect.xy;
}
