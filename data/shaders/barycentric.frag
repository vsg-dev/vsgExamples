#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_fragment_shader_barycentric : enable
layout(location = 0) out vec4 color;
void main()
{
    float margin = 0.02;
    if (min(min(gl_BaryCoordEXT.x, gl_BaryCoordEXT.y), gl_BaryCoordEXT.z) < margin)
    {
        color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float radius = 0.1;
    if (length(gl_BaryCoordEXT-vec3(0.333, 0.333, 0.333)) < radius)
    {
        color = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    color = vec4(gl_BaryCoordEXT, 1.0);
}
