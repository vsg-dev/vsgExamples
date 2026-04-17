#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

#pragma import_defines (DEPTHPEELING_PASS)

#define PEEL_DESCRIPTOR_SET 2

layout(input_attachment_index = 0, set = PEEL_DESCRIPTOR_SET, binding = 2) uniform subpassInput peelOutput;

layout(location = 0) out vec4 outColor;

void main()
{
    // (!) in case of final compose pass input is already premultiplied
    // in this case over-blending is done in fixed function pipeline by using blending operation
    outColor = subpassLoad(peelOutput);
    
#ifdef DEPTHPEELING_PASS
    // (!) premultiplied alpha because of under-blending-usage
    // under-blending is done in fixed function pipeline by using blending operation
    outColor.rgb *= outColor.a;
#endif
}
