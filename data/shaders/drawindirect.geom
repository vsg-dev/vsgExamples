#version 450

layout (points) in;
layout (triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec4 vertexColor[];
layout(location = 0) out vec4 color;

void main()
{
    vec4 position = gl_in[0].gl_Position;

    float size = position.z*100.0;

    gl_Position = position + vec4(size, -size, 0.0, 0.0);
    color = vec4(0.0, 1.0, 0.0, 1.0);
    EmitVertex();

    gl_Position = position + vec4(-size, -size, 0.0, 0.0);
    color = vec4(1.0, 0.0, 0.0, 1.0);
    EmitVertex();

    gl_Position = position + vec4(0.0, size, 0.0, 0.0);
    color = vec4(0.0, 0.0, 1.0, 1.0);
    EmitVertex();
    EndPrimitive();
}
