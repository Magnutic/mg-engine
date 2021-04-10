//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_defs.h
 * Compile-time constants and configuration.
 */

#pragma once

#include <cstdint>

/** Tweakable compile-time constants. */
namespace Mg::defs {

//--------------------------------------------------------------------------------------------------
// Log
//--------------------------------------------------------------------------------------------------

constexpr const char* k_engine_log_file = "mg_log.txt";

//--------------------------------------------------------------------------------------------------
// Window
//--------------------------------------------------------------------------------------------------

constexpr bool k_default_to_desktop_res_in_fullscreen = true;

// Default resolution for windowed mode:
constexpr uint32_t k_default_res_x = 1024;
constexpr uint32_t k_default_res_y = 768;

//--------------------------------------------------------------------------------------------------
// Rendering
//--------------------------------------------------------------------------------------------------

/** Maximum number of samplers associated with a material. Keep in mind that this is limited not
 * only by what the underlying API can provide (e.g. for OpenGL, at least 16 sampler units are
 * guaranteed), but also that rendering code might need to reserve some sampler units for internal
 * purposes.
 */
constexpr std::size_t k_max_samplers_per_material = 8;

/** Maximum number of boolean options per material. */
constexpr std::size_t k_max_options_per_material = 30;

/** Size of storage for material parameter values. */
constexpr std::size_t k_material_parameters_buffer_size = 128;

//--------------------------------------------------------------------------------------------------
// Light cluster grid configuration. The renderer divides the view frustum into a grid, each element
// of which holds a list of lights that intersect it. This allows faster light calculation.
// See mg_light_grid.h and mg_light_buffers.h

/** Maximum number of lights that may simultaneously affect a cluster. If more lights than this
 * number overlap a cluster, there will be artefacts. It is, however, best to keep this number
 * relatively small, to avoid growing data requirements.
 */
constexpr std::size_t max_lights_per_cluster = 128;

/** Maximum number of lights that may be rendered at a time. */
constexpr std::size_t max_num_lights = 512;

/** Width of light cluster grid. */
constexpr std::size_t light_grid_width = 16;

/** Height of light cluster grid. */
constexpr std::size_t light_grid_height = 8;

/** Depth of light cluster grid. */
constexpr std::size_t light_grid_depth = 24;

/** Depth at which the light grid ends. Lights beyond this will be inside the final grid slice. */
constexpr std::size_t light_grid_far_plane = 500;

/** Bias in depth slice calculation, used to avoid too many thin slices near the camera. */
constexpr float light_grid_depth_bias = -3.5f;

constexpr std::size_t light_grid_size = light_grid_width * light_grid_height * light_grid_depth;

} // namespace Mg::defs
