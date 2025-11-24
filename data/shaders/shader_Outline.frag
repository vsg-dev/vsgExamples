#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 shadowColor = {1.0,0.5,0.0,0.5 };  // Color of the shadow/outline

    vec2 texSize = textureSize(texSampler, 0);
    vec2 texOffset = vec2(1.0) / texSize; // Size of a single texel

    float shadowAlpha = 0.0;

    shadowAlpha = 0.0;
    int n=3; // Size of surrounding for outline sampling
    for (int x=-n; x<=n; ++x) {
      for (int y=-n; y<=n; ++y) {
          shadowAlpha += texture(texSampler,
                                 +fragTexCoord
                                 +vec2(x,y)*texOffset).a;
      }
    }

    // Get the original texture color
    vec4 original = texture(texSampler, fragTexCoord);

    // First combine the shadow with the background color
    outColor = mix(vec4(0.1,0.1,0.3,1.0), shadowColor, min(shadowAlpha,1.0));

    // Now combine with the original model
    outColor = mix(outColor, original, original.a);
}

