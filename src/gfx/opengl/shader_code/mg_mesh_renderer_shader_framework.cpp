//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_mesh_renderer_shader_framework.h"

#include <string_view>

using namespace std::literals;

namespace Mg::gfx::internal {

namespace {

constexpr const auto vertex_framework_code = R"(
layout(location = 0) in vec3 vert_position;
layout(location = 1) in vec2 vert_tex_coord;
layout(location = 2) in vec3 vert_normal;
layout(location = 3) in vec3 vert_tangent;
layout(location = 4) in vec3 vert_bitangent;
layout(location = 5) in vec4 vert_influences;
layout(location = 6) in vec4 vert_joint_weights;

layout(location = 8) in uint _matrix_index;

layout(std140) uniform MatrixBlock {
    mat4 m_matrices[MATRIX_ARRAY_SIZE];
    mat4 mvp_matrices[MATRIX_ARRAY_SIZE];
} _matrix_block;

#define MATRIX_M   (_matrix_block.m_matrices[_matrix_index])
#define MATRIX_MVP (_matrix_block.mvp_matrices[_matrix_index])

#if SKELETAL_ANIMATION_ENABLED

    layout(std140) uniform SkinningMatrixBlock {
        mat4 skinning_matrices[SKINNING_MATRIX_ARRAY_SIZE];
    } _skinning_matrix_block;

#    define SKINNING_MATRIX(index) (_skinning_matrix_block.skinning_matrices[index])

#endif // SKELETAL_ANIMATION_ENABLED

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
    vec2 tex_coord; // Primary texture coordinate
    vec3 position;  // Position (world space)
    vec3 normal;    // Surface normal (world space)
    mat3 TBN;       // Tangent space basis matrix
} vert_out;

struct VertexParams {
    vec3 position;
    vec2 tex_coord;
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
    v_o.tex_coord = vert_tex_coord;
    v_o.normal = vert_normal;
    v_o.tangent = vert_tangent;
    v_o.bitangent = vert_bitangent;

    vertex_preprocess(v_o);

#    define POSITION (v_o.position)
#    define TEX_COORD (v_o.tex_coord)
#    define NORMAL (v_o.normal)
#    define TANGENT (v_o.tangent)
#    define BITANGENT (v_o.bitangent)

#else

#    define POSITION (vert_position)
#    define TEX_COORD (vert_tex_coord)
#    define NORMAL (vert_normal)
#    define TANGENT (vert_tangent)
#    define BITANGENT (vert_bitangent)

#endif // VERTEX_PREPROCESS_ENABLED

    vert_out.tex_coord = TEX_COORD;

#if SKELETAL_ANIMATION_ENABLED

    vec3 position = vec3(0.0);
    vec3 tangent = vec3(0.0);
    vec3 bitangent = vec3(0.0);
    vec3 normal = vec3(0.0);

    for (uint i = 0u; i < 4u; ++i) {
        uint joint_index = uint(vert_influences[i]);
        float weight = vert_joint_weights[i];
        position  += (weight * SKINNING_MATRIX(joint_index) * vec4(POSITION,  1.0)).xyz;
        tangent   += (weight * SKINNING_MATRIX(joint_index) * vec4(TANGENT,   0.0)).xyz;
        bitangent += (weight * SKINNING_MATRIX(joint_index) * vec4(BITANGENT, 0.0)).xyz;
        normal    += (weight * SKINNING_MATRIX(joint_index) * vec4(NORMAL,    0.0)).xyz;
    }

#else

    vec3 position  = POSITION;
    vec3 tangent   = TANGENT;
    vec3 bitangent = BITANGENT;
    vec3 normal    = NORMAL;

#endif // SKELETAL_ANIMATION_ENABLED

    // Transform vertex location to projection space
    gl_Position = MATRIX_MVP * vec4(position, 1.0);

    // Pass through world-space position
    vert_out.position = (MATRIX_M * vec4(position, 1.0)).xyz;

    // Transform tangent space vectors
    tangent         = normalize(MATRIX_M * vec4(tangent,   0.0)).xyz;
    bitangent       = normalize(MATRIX_M * vec4(bitangent, 0.0)).xyz;
    vert_out.normal = normalize(MATRIX_M * vec4(normal,    0.0)).xyz;

    // Create tangent space basis matrix
    vert_out.TBN = mat3(tangent, bitangent, vert_out.normal);
}
)"sv;

constexpr const auto fragment_framework_code = R"(
in v2f {
    vec2 tex_coord; // Primary texture coordinate
    vec3 position;  // Position (world space)
    vec3 normal;    // Surface normal (world space)
    mat3 TBN;       // Tangent space basis matrix
} _frag_in;

layout (location = 0) out vec4 _frag_out;

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

#define WORLD_POSITION (_frag_in.position)
#define TEX_COORD (_frag_in.tex_coord)
#define SCREEN_POSITION (gl_FragCoord)
#define TANGENT_TO_WORLD (_frag_in.TBN)

// Definition of light source.
struct Light {
   vec4 vector;
   vec3 colour;
   float range_sqr;
};

layout(std140) uniform LightBlock {
    Light lights[MAX_NUM_LIGHTS];
} _light_block;

struct LightInput {
    vec3 direction;
    vec3 colour;
    float range_sqr;
    float distance_sqr;
};

struct SurfaceParams {
    vec3 albedo;
    vec3 specular;
    vec3 normal;
    vec3 emission;
    float occlusion;
    float gloss;
    float alpha;
};

float attenuate(float distance_sqr, float range_sqr_reciprocal);

// User-defined light function
vec3 light(const LightInput light, const SurfaceParams surface, const vec3 view_direction);

uniform usamplerBuffer _sampler_tile_data;
uniform usamplerBuffer _sampler_light_index;

// Light loop used for clustered forward rendering
vec3 _light_accumulate(const SurfaceParams surface, const vec3 view_direction)
{
    vec3 result = vec3(0.0);

    vec2 tile_dims = vec2(float(_frame_block.viewport_size.x) / LIGHT_GRID_WIDTH,
                          float(_frame_block.viewport_size.y) / LIGHT_GRID_HEIGHT);

    float z = 2.0 * gl_FragCoord.z - 1.0;

    int slice = int(log2(z * _frame_block.cluster_grid_params.z_param.x
                         + _frame_block.cluster_grid_params.z_param.y)
                    * _frame_block.cluster_grid_params.scale
                    + _frame_block.cluster_grid_params.bias);

    slice = clamp(slice, 0, int(LIGHT_GRID_DEPTH) - 1);

    ivec3 l = ivec3(ivec2(SCREEN_POSITION.xy / tile_dims), slice);

    int tile_index = l.z * int(LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT)
                   + l.y * int(LIGHT_GRID_WIDTH)
                   + l.x;

    uint light_offset = texelFetch(_sampler_tile_data, tile_index).r;
    uint light_amount = texelFetch(_sampler_tile_data, tile_index).g;

    for (uint i = 0u; i < light_amount; ++i) {
        uint light_index = texelFetch(_sampler_light_index, int(light_offset + i)).r;
        Light ls         = _light_block.lights[light_index];

        LightInput li;
        li.direction    = ls.vector.xyz - WORLD_POSITION;
        li.distance_sqr = dot(li.direction, li.direction);

        if (li.distance_sqr >= ls.range_sqr) { continue; }

        li.range_sqr = ls.range_sqr;
        li.direction = normalize(li.direction);
        li.colour    = ls.colour;

        result += light(li, surface, view_direction);
    }

    return result;
}

// Input to user-defined surface-function, likely to be expanded as needed.
struct SurfaceInput {
    vec3 view_direction;
};

void surface(const SurfaceInput s_in, out SurfaceParams s_out);

void final_colour(const SurfaceInput s_in, const SurfaceParams s, inout vec4 colour);

void main() {
    SurfaceInput s_in;
    s_in.view_direction = normalize(CAMERA_POSITION - WORLD_POSITION);
    SurfaceParams s;
    surface(s_in, s);

    vec4 colour = vec4(s.emission + _light_accumulate(s, s_in.view_direction), s.alpha);
    final_colour(s_in, s, colour);

    // Output colour with alpha = 1.0
    // We do exposure here instead of in post-processing to deal with 16-bit float range
    _frag_out = vec4(pow(2.0, _frame_block.exposure) * colour.rgb, colour.a);
}
)"sv;

void add_define(std::string& string, const std::string_view define_name, size_t value)
{
    string += "#define ";
    string += define_name;
    string += ' ';
    string += std::to_string(value);
    string += '\n';
}

} // namespace

std::string
mesh_renderer_vertex_shader_framework_code(const MeshRendererFrameworkShaderParams& params)
{
    std::string result;
    result.reserve(vertex_framework_code.size() + 500);

    result += "#version 330 core\n";
    add_define(result, "MATRIX_ARRAY_SIZE", params.matrix_array_size);
    add_define(result, "SKINNING_MATRIX_ARRAY_SIZE", params.skinning_matrix_array_size);
    if (params.skinning_matrix_array_size > 0) {
        add_define(result, "SKELETAL_ANIMATION_ENABLED", 1);
    }

    result += vertex_framework_code;

    return result;
}

std::string
mesh_renderer_fragment_shader_framework_code(const MeshRendererFrameworkShaderParams& params)
{
    std::string result;
    result.reserve(fragment_framework_code.size() + 500);

    result += "#version 330 core\n";
    add_define(result, "MAX_NUM_LIGHTS", params.max_num_lights);
    add_define(result, "LIGHT_GRID_WIDTH", params.light_grid_width);
    add_define(result, "LIGHT_GRID_HEIGHT", params.light_grid_height);
    add_define(result, "LIGHT_GRID_DEPTH", params.light_grid_depth);

    result += fragment_framework_code;

    return result;
}

} // namespace Mg::gfx::internal
