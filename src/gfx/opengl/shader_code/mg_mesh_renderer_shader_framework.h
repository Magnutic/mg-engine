//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_mesh_renderer_shader_framework.h
 * The 'framework' shader code -- the code which defines the interface between renderer and the
 * material-specific shader code -- for MeshRenderer.
 */

#pragma once

// contains config macros used in generating shader code
#include "mg/gfx/mg_light_buffers.h"
#include "mg/gfx/mg_light_grid.h"
#include "mg/gfx/mg_matrix_uniform_handler.h"

#define MG_STRINGISE_HELPER(x) #x
#define MG_STRINGISE(x) MG_STRINGISE_HELPER(x)

#define LIGHT_GRID_WIDTH_STR MG_STRINGISE(MG_LIGHT_GRID_WIDTH)
#define LIGHT_GRID_HEIGHT_STR MG_STRINGISE(MG_LIGHT_GRID_HEIGHT)
#define LIGHT_GRID_DEPTH_STR MG_STRINGISE(MG_LIGHT_GRID_DEPTH)
#define LIGHT_GRID_FAR_PLANE_STR MG_STRINGISE(MG_LIGHT_GRID_FAR_PLANE)

#define MAX_NUM_LIGHTS_STR MG_STRINGISE(MG_MAX_NUM_LIGHTS)
#define MAX_LIGHTS_PER_CLUSTER_STR MG_STRINGISE(MAX_LIGHTS_PER_CLUSTER)

#define MATRIX_UBO_ARRAY_SIZE_STR MG_STRINGISE(MATRIX_UBO_ARRAY_SIZE)

namespace Mg::gfx::shader_code::mesh_renderer {

static constexpr const char k_shader_framework_vertex_code[] = R"(
#version 330 core

#define MATRIX_ARRAY_SIZE )" MATRIX_UBO_ARRAY_SIZE_STR R"(

layout(location = 0) in vec3 vert_position;
layout(location = 1) in vec2 vert_uv0;
layout(location = 2) in vec2 vert_uv1;
layout(location = 3) in vec3 vert_normal;
layout(location = 4) in vec3 vert_tangent;
layout(location = 5) in vec3 vert_bitangent;
layout(location = 6) in uvec4 vert_joint_id;
layout(location = 7) in uvec4 vert_joint_weight;

layout(location = 8) in uint _matrix_index;

layout(std140) uniform MatrixBlock {
    mat4 m_matrices[MATRIX_ARRAY_SIZE];
    mat4 mvp_matrices[MATRIX_ARRAY_SIZE];
} _matrix_block;

#define MATRIX_M   (_matrix_block.m_matrices[_matrix_index])
#define MATRIX_MVP (_matrix_block.mvp_matrices[_matrix_index])

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
)";

static constexpr const char k_shader_framework_fragment_code[] = R"(
#version 330 core

#define LIGHT_GRID_WIDTH )" LIGHT_GRID_WIDTH_STR R"(
#define LIGHT_GRID_HEIGHT )" LIGHT_GRID_HEIGHT_STR R"(
#define LIGHT_GRID_DEPTH )" LIGHT_GRID_DEPTH_STR R"(
#define LIGHT_GRID_FAR_PLANE )" LIGHT_GRID_FAR_PLANE_STR R"(
#define MAX_NUM_LIGHTS )" MAX_NUM_LIGHTS_STR R"(
#define MAX_LIGHTS_PER_CLUSTER )" MAX_LIGHTS_PER_CLUSTER_STR R"(

in v2f {
    vec2 uv0;      // Primary UV coordinate
    vec2 uv1;      // Secondary UV coordinate
    vec3 position; // Position (world space)
    vec3 normal;   // Surface normal (world space)
    mat3 TBN;      // Tangent space basis matrix
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
#define UV_COORD0 (_frag_in.uv0)
#define UV_COORD1 (_frag_in.uv1)
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
)";

} // namespace Mg::gfx::shader_code::mesh_renderer
