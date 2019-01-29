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

#include "mg_lit_mesh_shader_factory.h"

#include "mg/core/mg_resource_cache.h"
#include "mg/core/mg_root.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_light_grid.h"
#include "mg/gfx/mg_material.h"
#include "mg/resources/mg_shader_resource.h"

#include "../mg_opengl_shader.h"
#include "../mg_glad.h"

#include "shader_code/mg_lit_mesh_framework_shader_code.h"

namespace Mg::gfx::mesh_renderer {

FrameBlock make_frame_block(const ICamera& camera)
{
    std::array<int, 4> viewport_data{};
    glGetIntegerv(GL_VIEWPORT, &viewport_data[0]);
    glm::uvec2 viewport_size{ narrow<uint32_t>(viewport_data[2]),
                              narrow<uint32_t>(viewport_data[3]) };

    static const float scale = MG_LIGHT_GRID_DEPTH / std::log2(float(MG_LIGHT_GRID_FAR_PLANE));

    const auto depth_range = camera.depth_range();
    const auto z_near      = depth_range.near();
    const auto z_far       = depth_range.far();
    const auto c           = std::log2(2.0f * z_far * z_near);

    FrameBlock frame_block;
    frame_block.camera_position_and_time =
        glm::vec4(camera.get_position(), float(Root::time_since_init()));
    frame_block.viewport_size = viewport_size;

    frame_block.cluster_grid_params.z_param = glm::vec2(z_near - z_far, z_near + z_far);
    frame_block.cluster_grid_params.scale   = -scale;
    frame_block.cluster_grid_params.bias    = float(MG_LIGHT_GRID_DEPTH_BIAS) + c * scale;

    frame_block.camera_exposure = -7.0;

    return frame_block;
}

} // namespace Mg::gfx::mesh_renderer

namespace Mg::gfx {

ShaderCode MeshShaderProvider::make_shader_code(const Material& material) const
{
    constexpr size_t     code_reservation_size = 5 * 1024;
    constexpr const char version_tag[]         = "#version 330 core\n";

    std::string vertex_code;
    vertex_code.reserve(code_reservation_size);

    std::string fragment_code;
    fragment_code.reserve(code_reservation_size);

    // GLSL version tag
    vertex_code += version_tag;
    fragment_code += version_tag;

    // Access shader resource
    auto shader_resource_access = material.shader().access();

    // If there is a vertex-preprocess function, then include the corresponding #define
    if ((shader_resource_access->tags() & ShaderTag::DEFINES_VERTEX_PREPROCESS) != 0) {
        vertex_code += "#define VERTEX_PREPROCESS_ENABLED 1";
    }

    // Include framework shader code.
    vertex_code += k_lit_mesh_framework_vertex_code;
    fragment_code += k_lit_mesh_framework_fragment_code;

    // Include sampler, parameter, and enabled-option definitions.
    vertex_code += shader_interface_code(material);
    fragment_code += shader_interface_code(material);

    // Include resource-defined shader code.
    vertex_code += shader_resource_access->vertex_code();
    fragment_code += shader_resource_access->fragment_code();

    return ShaderCode{ std::move(vertex_code), std::move(fragment_code) };
}

void MeshShaderProvider::setup_shader_state(ShaderProgram& program, const Material& material) const
{
    opengl::use_program(program);
    opengl::set_uniform_block_binding(
        program, "MatrixBlock", UniformBufferSlot{ mesh_renderer::k_matrix_ubo_index });
    opengl::set_uniform_block_binding(
        program, "FrameBlock", UniformBufferSlot{ mesh_renderer::k_frame_ubo_index });
    opengl::set_uniform_block_binding(
        program, "LightBlock", UniformBufferSlot{ mesh_renderer::k_light_ubo_index });
    opengl::set_uniform_block_binding(
        program, "MaterialParams", UniformBufferSlot{ mesh_renderer::k_material_params_ubo_index });

    int32_t tex_unit = 0;

    for (const Material::Sampler& sampler : material.samplers()) {
        opengl::set_uniform(opengl::uniform_location(program, sampler.name.str_view()), tex_unit++);
    }

    opengl::set_uniform(opengl::uniform_location(program, "_sampler_tile_data"),
                        mesh_renderer::k_sampler_tile_data_index);

    opengl::set_uniform(opengl::uniform_location(program, "_sampler_light_index"),
                        mesh_renderer::k_sampler_light_index_index);
}

} // namespace Mg::gfx
