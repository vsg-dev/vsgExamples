#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (NUM_CLIPS)

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(set = 1, binding = 0) uniform ClipSettings {
    vec4 sphere; // x,y,z, radius
} clipSettings;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
    #if NUM_CLIPS > 0
        float gl_ClipDistance[NUM_CLIPS];
    #endif
    //float gl_CullDistance[1];
};

void main() {
    gl_Position = (pc.projection * pc.modelview) * vec4(inPosition, 1.0);

#if 0
    fragColor = vec4(inColor, 1.0);
#else
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
#endif

    fragTexCoord = inTexCoord;

    vec4 eye_Position = pc.modelview * vec4(inPosition, 1.0);

    #if NUM_CLIPS > 0
        for (uint cOffset = 0; cOffset < NUM_CLIPS; ++cOffset) {
            if (cOffset == 0) {
                // clip volume inside the sphere using eye coordinates
                gl_ClipDistance[cOffset] = (length(eye_Position.xyz - clipSettings.sphere.xyz) - clipSettings.sphere.w);
            } else if (cOffset == 1) {
                // simple clip on world coordinate X less than zero
               gl_ClipDistance[cOffset] = inPosition.x;
            }
        }
    #endif
    // gl_CullDistance[0] = (length(inPosition - clipSettings.sphere.xyz) - 3.0);
}
