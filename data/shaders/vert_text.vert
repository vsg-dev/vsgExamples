#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
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
    vec3 billboardAxis;
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

    vec3 vert = vec3(offset + size, 0.0);
	
	mat4 instPostionMat = mat4(1.0); 
	instPostionMat[3] = vec4(instPostion, 1.0);
	
	mat4 modelView = pc.modelview  * instPostionMat;
	
	// xaxis
	if(textMetrics.billboardAxis.x > 0.0)
	{
		modelView[0][0] = 1.0;
		modelView[0][1] = 0.0;
		modelView[0][2] = 0.0;
	}

	// yaxis
	if(textMetrics.billboardAxis.y > 0.0)
	{
		modelView[1][0] = 0.0;
		modelView[1][1] = 1.0;
		modelView[1][2] = 0.0;
	}
	
	// zaxis
	if(textMetrics.billboardAxis.z > 0.0)
	{
		modelView[2][0] = 0.0;
		modelView[2][1] = 0.0;
		modelView[2][2] = 1.0;
	}
	
    gl_Position = (pc.projection * modelView) * vec4(vert, 1.0);
    
    fragTexCoord = (inPosition.xy * glyphUvRect.zw) + glyphUvRect.xy;
}
