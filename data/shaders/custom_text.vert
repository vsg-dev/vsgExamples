#version 450
#extension GL_ARB_separate_shader_objects : enable

#pragma import_defines (CPU_LAYOUT, BILLBOARD_AND_SCALE)

// CPU layout provides all vertex data
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inOutlineColor;
layout(location = 3) in float inOutlineWidth;
layout(location = 4) in vec3 inTexCoord;

#ifdef BILLBOARD_AND_SCALE
layout(set = 0, binding = 1) uniform BillboardUniform {
    float scale;
} billboard;
#endif

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 outlineColor;
layout(location = 2) out float outlineWidth;
layout(location = 3) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {

#ifdef BILLBOARD_AND_SCALE
    mat4 modelview = mat4(billboard.scale, 0.0, 0.0, 0.0,
                          0.0, billboard.scale, 0.0, 0.0,
                          0.0, 0.0, billboard.scale, 0.0,
                          pc.modelview[3][0], pc.modelview[3][1], pc.modelview[3][2], 1.0);
#else
    mat4 modelview = pc.modelview;
#endif


// CPU layout provides all vertex data
    gl_Position = (pc.projection * modelview) * vec4(inPosition, 1.0);
    gl_Position.z -= inTexCoord.z*0.001;
    fragColor = inColor;
    outlineColor = inOutlineColor;
    outlineWidth = inOutlineWidth;
    fragTexCoord = inTexCoord.xy;
}
