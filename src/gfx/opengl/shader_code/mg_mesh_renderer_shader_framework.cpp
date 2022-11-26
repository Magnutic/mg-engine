//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_mesh_renderer_shader_framework.h"
#include "mg/gfx/mg_joint.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/utils/mg_string_utils.h"

#include <algorithm>
#include <string_view>

using namespace std::literals;

namespace Mg::gfx::internal {

namespace {

constexpr const auto vertex_fragment_common_code = R"(
struct ClusterGridParams {
    vec2 z_param;
    float scale;
    float bias;
};

layout(std140) uniform FrameBlock {
    ClusterGridParams cluster_grid_params;
    vec4 camera_position_and_time;
    uvec2 viewport_size;
    float z_near;
    float z_far;

    float exposure;
} _frame_block;

)";

constexpr const auto vertex_framework_code = R"(
layout(location = POSITION_BINDING_LOCATION) in vec3 vert_position;
layout(location = TEX_COORD_BINDING_LOCATION) in vec2 vert_tex_coord;
layout(location = NORMAL_BINDING_LOCATION) in vec3 vert_normal;
layout(location = TANGENT_BINDING_LOCATION) in vec3 vert_tangent;
layout(location = BITANGENT_BINDING_LOCATION) in vec3 vert_bitangent;
layout(location = JOINT_INFLUENCES_BINDING_LOCATION) in vec4 vert_joint_influences;
layout(location = JOINT_WEIGHTS_BINDING_LOCATION) in vec4 vert_joint_weights;

layout(location = MATRIX_INDEX_BINDING_LOCATION) in uint _matrix_index;

layout(std140) uniform MatrixBlock {
    mat4 m_matrices[MATRIX_ARRAY_SIZE];
    mat4 vp_matrices[MATRIX_ARRAY_SIZE];
} _matrix_block;

#define MATRIX_M   (_matrix_block.m_matrices[_matrix_index])
#define MATRIX_VP (_matrix_block.vp_matrices[_matrix_index])

#if SKELETAL_ANIMATION_ENABLED

    layout(std140) uniform SkinningMatrixBlock {
        mat4 skinning_matrices[SKINNING_MATRIX_ARRAY_SIZE];
    } _skinning_matrix_block;

#    define SKINNING_MATRIX(index) (_skinning_matrix_block.skinning_matrices[index])

#endif // SKELETAL_ANIMATION_ENABLED

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

    // Loop manually unrolled as work around for glitches appearing on Intel HD Graphics 530 on Linux.

    // Note: MATRIX_M is already multiplied into the skinning matrices.
    position        += (vert_joint_weights[0] * SKINNING_MATRIX(uint(vert_joint_influences[0])) * vec4(POSITION,  1.0)).xyz;
    tangent         += (vert_joint_weights[0] * SKINNING_MATRIX(uint(vert_joint_influences[0])) * vec4(TANGENT,   0.0)).xyz;
    bitangent       += (vert_joint_weights[0] * SKINNING_MATRIX(uint(vert_joint_influences[0])) * vec4(BITANGENT, 0.0)).xyz;
    vert_out.normal += (vert_joint_weights[0] * SKINNING_MATRIX(uint(vert_joint_influences[0])) * vec4(NORMAL,    0.0)).xyz;

    position        += (vert_joint_weights[1] * SKINNING_MATRIX(uint(vert_joint_influences[1])) * vec4(POSITION,  1.0)).xyz;
    tangent         += (vert_joint_weights[1] * SKINNING_MATRIX(uint(vert_joint_influences[1])) * vec4(TANGENT,   0.0)).xyz;
    bitangent       += (vert_joint_weights[1] * SKINNING_MATRIX(uint(vert_joint_influences[1])) * vec4(BITANGENT, 0.0)).xyz;
    vert_out.normal += (vert_joint_weights[1] * SKINNING_MATRIX(uint(vert_joint_influences[1])) * vec4(NORMAL,    0.0)).xyz;

    position        += (vert_joint_weights[2] * SKINNING_MATRIX(uint(vert_joint_influences[2])) * vec4(POSITION,  1.0)).xyz;
    tangent         += (vert_joint_weights[2] * SKINNING_MATRIX(uint(vert_joint_influences[2])) * vec4(TANGENT,   0.0)).xyz;
    bitangent       += (vert_joint_weights[2] * SKINNING_MATRIX(uint(vert_joint_influences[2])) * vec4(BITANGENT, 0.0)).xyz;
    vert_out.normal += (vert_joint_weights[2] * SKINNING_MATRIX(uint(vert_joint_influences[2])) * vec4(NORMAL,    0.0)).xyz;

    position        += (vert_joint_weights[3] * SKINNING_MATRIX(uint(vert_joint_influences[3])) * vec4(POSITION,  1.0)).xyz;
    tangent         += (vert_joint_weights[3] * SKINNING_MATRIX(uint(vert_joint_influences[3])) * vec4(TANGENT,   0.0)).xyz;
    bitangent       += (vert_joint_weights[3] * SKINNING_MATRIX(uint(vert_joint_influences[3])) * vec4(BITANGENT, 0.0)).xyz;
    vert_out.normal += (vert_joint_weights[3] * SKINNING_MATRIX(uint(vert_joint_influences[3])) * vec4(NORMAL,    0.0)).xyz;

#else

    vec3 position   = (MATRIX_M * vec4(POSITION,  1.0)).xyz;
    vec3 tangent    = (MATRIX_M * vec4(TANGENT,   0.0)).xyz;
    vec3 bitangent  = (MATRIX_M * vec4(BITANGENT, 0.0)).xyz;
    vert_out.normal = (MATRIX_M * vec4(NORMAL,    0.0)).xyz;

#endif // SKELETAL_ANIMATION_ENABLED

    // Transform vertex location to projection space
    gl_Position = MATRIX_VP * vec4(position, 1.0);

    // Pass through world-space position
    vert_out.position = position.xyz;

    // Create tangent space basis matrix
    vert_out.TBN = mat3(normalize(tangent), normalize(bitangent), normalize(vert_out.normal));
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

#define CAMERA_POSITION (_frame_block.camera_position_and_time.xyz)
#define CAMERA_EXPOSURE (_frame_block.exposure)
#define TIME (_frame_block.camera_position_and_time.w)
#define VIEWPORT_SIZE (_frame_block.viewport_size)
#define ZNEAR (_frame_block.z_near)
#define ZFAR (_frame_block.z_far)

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

    // Find in which cluster this fragment is. The view frustum is subdivided into clusters, each of
    // which has its own list of light sources that can affect fragments within the cluster.

    // First, find which tile -- the screen is subdivided into equally-sized tiles.
    vec2 tile_dims = vec2(float(_frame_block.viewport_size.x) / LIGHT_GRID_WIDTH,
                          float(_frame_block.viewport_size.y) / LIGHT_GRID_HEIGHT);

    // Then, find the depth 'slice'.
    float z = 2.0 * gl_FragCoord.z - 1.0;
    int slice = int(log2(z * _frame_block.cluster_grid_params.z_param.x
                         + _frame_block.cluster_grid_params.z_param.y)
                    * _frame_block.cluster_grid_params.scale
                    + _frame_block.cluster_grid_params.bias);
    slice = clamp(slice, 0, int(LIGHT_GRID_DEPTH) - 1);

    // Tile location and depth slice together define the cluster.
    ivec3 l = ivec3(ivec2(SCREEN_POSITION.xy / tile_dims), slice);
    int cluster_index = l.z * int(LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT)
                      + l.y * int(LIGHT_GRID_WIDTH)
                      + l.x;

    // Where in the lights block will we find the lights for this cluster?
    uint light_index_array_offset = texelFetch(_sampler_tile_data, cluster_index).r;
    uint num_lights_in_cluster = texelFetch(_sampler_tile_data, cluster_index).g;

    for (uint i = 0u; i < num_lights_in_cluster; ++i) {
        uint light_index = texelFetch(_sampler_light_index, int(light_index_array_offset + i)).r;
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

void add_define(std::string& string, const std::string_view define_name, const size_t value)
{
    string += "#define ";
    string += define_name;
    string += ' ';
    string += std::to_string(value);
    string += '\n';
}

struct VertexAttributeBinding {
    const char* identifier;
    uint32_t location;
};

VertexAttributeBinding from_vertex_attribute(const VertexAttribute& attribute)
{
    return { .identifier = attribute.identifier, .location = attribute.binding_location };
}

// Add #defines that configure the shader to match the vertex format.
void add_vertex_attribute_binding_defines(std::string& string,
                                          std::vector<VertexAttributeBinding> bindings)
{
    // Check for duplicate binding locations.
    {
        auto compare_binding_location = [](const auto& l, const auto& r) {
            return l.location < r.location;
        };
        std::ranges::sort(bindings, compare_binding_location);

        auto same_binding_location = [](const auto& l, const auto& r) {
            return l.location == r.location;
        };
        auto it = std::ranges::adjacent_find(bindings, same_binding_location);
        const bool has_duplicate_binding_locations = it != bindings.end();

        if (has_duplicate_binding_locations) {
            const auto& [identifier_1, location_1] = *it;
            const auto& [identifier_2, location_2] = *std::next(it);
            throw RuntimeError{
                "Duplicate vertex attribute binding locations: attributes '{}' and '{}' both have "
                "binding location {}",
                identifier_1,
                identifier_2,
                location_1
            };
        }
    }

    for (const auto& [identifier, location] : bindings) {
        add_define(string, to_upper(identifier) + "_BINDING_LOCATION", location);
    }
}

} // namespace

std::string
mesh_renderer_vertex_shader_framework_code(const MeshRendererFrameworkShaderParams& params)
{
    std::string result;
    result.reserve(vertex_framework_code.size() + 500);

    result += "#version 330 core\n";
    result += vertex_fragment_common_code;
    add_define(result, "MATRIX_ARRAY_SIZE", params.matrix_array_size);
    add_define(result, "SKINNING_MATRIX_ARRAY_SIZE", params.skinning_matrix_array_size);
    if (params.skinning_matrix_array_size > 0) {
        add_define(result, "SKELETAL_ANIMATION_ENABLED", 1);
    }

    // Add #defines for vertex attribute binding locations.
    {
        std::vector<VertexAttributeBinding> bindings;
        std::ranges::transform(Mesh::vertex_attributes,
                               std::back_inserter(bindings),
                               from_vertex_attribute);
        std::ranges::transform(Mesh::influences_attributes,
                               std::back_inserter(bindings),
                               from_vertex_attribute);
        bindings.push_back({ "MATRIX_INDEX", params.matrix_index_vertex_attrib_binding_location });
        add_vertex_attribute_binding_defines(result, bindings);
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
    result += vertex_fragment_common_code;

    // Defines that configure the shader to match the given params.
    add_define(result, "MAX_NUM_LIGHTS", params.light_grid_config.max_num_lights);
    add_define(result, "LIGHT_GRID_WIDTH", params.light_grid_config.grid_width);
    add_define(result, "LIGHT_GRID_HEIGHT", params.light_grid_config.grid_height);
    add_define(result, "LIGHT_GRID_DEPTH", params.light_grid_config.grid_depth);

    result += fragment_framework_code;

    return result;
}

} // namespace Mg::gfx::internal
