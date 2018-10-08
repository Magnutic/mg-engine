layout(location = 0) in vec3 vert_position;
layout(location = 1) in vec2 vert_uv0;
layout(location = 2) in vec2 vert_uv1;
layout(location = 3) in vec3 vert_normal;
layout(location = 4) in vec3 vert_tangent;
layout(location = 5) in vec3 vert_bitangent;
layout(location = 6) in uvec4 vert_joint_id;
layout(location = 7) in uvec4 vert_joint_weight;

layout(location = 8) in uint _matrix_index;

struct matrices_t {
    mat4 MVP;
    mat4 M;
};

layout(std140) uniform MatrixBlock {
    matrices_t matrices[MATRIX_ARRAY_SIZE];
} _matrix_block;

#define MATRIX_MVP (_matrix_block.matrices[_matrix_index].MVP)
#define MATRIX_M (_matrix_block.matrices[_matrix_index].M)

struct ClusterGridParams {
    vec2 z_param;
    float scale;
    float bias;
};

layout(std140) uniform FrameBlock {
    ClusterGridParams cluster_grid_params;
    vec4 camera_position_and_time;
    uvec2 viewport_size;

    float exposure;
} _frame_block;

#define CAMERA_POSITION (_frame_block.camera_position_and_time.xyz)
#define CAMERA_EXPOSURE (_frame_block.exposure)
#define TIME (_frame_block.camera_position_and_time.w)
#define VIEWPORT_SIZE (_frame_block.viewport_size)

out v2f {
    vec2 uv0;      // Primary UV coordinate
    vec2 uv1;      // Secondary UV coordinate
    vec3 position; // Position (world space)
    vec3 normal;   // Surface normal (world space)
    mat3 TBN;      // Tangent space basis matrix
} vert_out;

struct VertexParams {
    vec3 position;
    vec2 uv0;
    vec2 uv1;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
};

#if VERTEX_PREPROCESS_ENABLED
void vertex_preprocess(inout VertexParams v_out);
#endif

// Basic model vertex shader
void main()
{
#if VERTEX_PREPROCESS_ENABLED
    VertexParams v_o;
    v_o.position = vert_position;
    v_o.uv0 = vert_uv0;
    v_o.uv1 = vert_uv1;
    v_o.normal = vert_normal;
    v_o.tangent = vert_tangent;
    v_o.bitangent = vert_bitangent;

    vertex_preprocess(v_o);

#define POSITION (v_o.position)
#define UV0 (v_o.uv0)
#define UV1 (v_o.uv1)
#define NORMAL (v_o.normal)
#define TANGENT (v_o.tangent)
#define BITANGENT (v_o.bitangent)

#else

#define POSITION (vert_position)
#define UV0 (vert_uv0)
#define UV1 (vert_uv1)
#define NORMAL (vert_normal)
#define TANGENT (vert_tangent)
#define BITANGENT (vert_bitangent)

#endif // VERTEX_PREPROCESS_ENABLED

    // Transform vertex location to projection space
    gl_Position = MATRIX_MVP * vec4(POSITION, 1.0);
    vert_out.uv0 = UV0;
    vert_out.uv1 = UV1;

    // Pass through world-space position
    vert_out.position = (MATRIX_M * vec4(POSITION, 1.0)).xyz;

    // Transform tangent space vectors
    vec3 tangent    = normalize(MATRIX_M * vec4(TANGENT,   0.0)).xyz;
    vec3 bitangent  = normalize(MATRIX_M * vec4(BITANGENT, 0.0)).xyz;
    vert_out.normal = normalize(MATRIX_M * vec4(NORMAL,    0.0)).xyz;

    // Create tangent space basis matrix
    vert_out.TBN = mat3(tangent, bitangent, vert_out.normal);
}

