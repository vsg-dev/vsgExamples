#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_fragment_shader_barycentric : enable
layout(location = 0) out vec4 color;
void main()
{
    color = vec4(gl_BaryCoordEXT, 1.0);
}
