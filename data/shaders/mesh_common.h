// include into shader *after* #pragma import_defines

#ifndef MESH_SUBGROUP_COUNT
#define MESH_SUBGROUP_COUNT 4
#endif

#ifndef MESH_SUBGROUP_SIZE
#define MESH_SUBGROUP_SIZE 32
#endif

#ifndef MESH_WORKGROUP_SIZE
#define MESH_WORKGROUP_SIZE MESH_SUBGROUP_COUNT * MESH_SUBGROUP_SIZE
#endif

#ifndef MAX_VERTEX_COUNT
#define MAX_VERTEX_COUNT 64
#endif

#ifndef MAX_PRIMITIVE_COUNT
#define MAX_PRIMITIVE_COUNT 126
#endif

#ifndef MESHLET_PER_TASK
#define MESHLET_PER_TASK 32
#endif

// vertex{11,11,11}, normal_polar{8,7}, tecoord{8,8} ->  64bits
struct PackedVertex
{
    uint64_t data;
};

struct Meshlet
{
    uint vertex_offset;
    uint triangle_offset;
    uint vertex_count;
    uint triangle_count;
};

struct Mesh
{
    uint count[4];
    vec4 originScale;
    vec4 texcoodExtents;
};

#define VIEW_DESCRIPTOR_SET 0
#define MATERIAL_DESCRIPTOR_SET 1
#define MESH_DESCRIPTOR_SET 2

#if defined(VSG_TEXTURECOORD_3)
#define VSG_TEXCOORD_COUNT 4
#elif defined(VSG_TEXTURECOORD_2)
#define VSG_TEXCOORD_COUNT 3
#elif defined(VSG_TEXTURECOORD_1)
#define VSG_TEXCOORD_COUNT 2
#else
#define VSG_TEXCOORD_COUNT 1
#endif

