#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelView;
} pc;

layout(location = 0) in vec4 vsg_Vertex;

layout(location = 0) out vec4 vertexColor;

out gl_PerVertex{
    vec4 gl_Position;
    float gl_PointSize;
};


struct Frustum
{
    vec4 left;
    vec4 right;
    vec4 top;
    vec4 bottom;
};

Frustum computeFrustum()
{
    mat4 pvm = pc.projection * pc.modelView;

    Frustum f;
    f.left = vec4(pvm[0][0] + pvm[0][3], pvm[1][0] + pvm[1][3], pvm[2][0] + pvm[2][3], pvm[3][0] + pvm[3][3]);
    f.right = vec4(-pvm[0][0] + pvm[0][3], -pvm[1][0] + pvm[1][3], -pvm[2][0] + pvm[2][3], -pvm[3][0] + pvm[3][3]);
    f.top = vec4(pvm[0][1] + pvm[0][3], pvm[1][1] + pvm[1][3], pvm[2][1] + pvm[2][3], pvm[3][1] + pvm[3][3]);
    f.bottom = vec4(-pvm[0][1] + pvm[0][3], -pvm[1][1] + pvm[1][3], -pvm[2][1] + pvm[2][3], -pvm[3][1] + pvm[3][3]);

    f.left = f.left / length(f.left.xyz);
    f.right = f.right / length(f.right.xyz);
    f.top = f.top / length(f.top.xyz);
    f.bottom = f.bottom / length(f.bottom.xyz);
    return f;
}

void main()
{
    vec4 vertex = vec4(vsg_Vertex.xyz, 1.0);
    gl_Position = (pc.projection * pc.modelView) * vertex;
    vec3 eyePos = vec4(pc.modelView * vertex).xyz;
    gl_PointSize = 50.0 / abs(eyePos.z);


    float distance = -eyePos.z;

    if (distance > 0.0) vertexColor = vec4(1.0, 1.0, 0.0, 1.0);
    else vertexColor = vec4(1.0, 0.0, 0.0, 1.0);

    vertexColor = vec4(0.0, 0.0, 0.0, 1.0);

    Frustum f = computeFrustum();
    vertexColor.r = dot(vertex, f.left);
    vertexColor.g = dot(vertex, f.right);
    vertexColor.b = dot(vertex, f.top);
}
