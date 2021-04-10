//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_renderer_shader_framework.h
 * The 'framework' shader code -- the code which defines the interface between renderer and the
 * material-specific shader code -- for MeshRenderer.
 */

#pragma once

#include "mg/gfx/mg_matrix_uniform_handler.h"

#include <string>

namespace Mg::gfx::internal {

struct MeshRendererFrameworkShaderParams {
    // Size of arrays of M (model-to-world) and MVP (model-to-perspective-projection) matrices.
    size_t matrix_array_size;

    // Size of array of skinning matrices for animated meshes. If 0, skinning animation is disabled.
    size_t skinning_matrix_array_size;

    // Size of array of light sources.
    size_t max_num_lights;

    // Parameters controlling the light grid for clustered rendering.
    size_t light_grid_width;
    size_t light_grid_height;
    size_t light_grid_depth;
};

std::string
mesh_renderer_vertex_shader_framework_code(const MeshRendererFrameworkShaderParams& params);
std::string
mesh_renderer_fragment_shader_framework_code(const MeshRendererFrameworkShaderParams& params);

} // namespace Mg::gfx::internal
