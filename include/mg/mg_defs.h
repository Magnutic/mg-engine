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

/** @file mg_defs.h
 * Compile-time constants and configuration.
 */

#pragma once

#include <cstdint>

/** Tweakable compile-time constants. */
namespace Mg::defs {

//--------------------------------------------------------------------------------------------------
// Root
//--------------------------------------------------------------------------------------------------

#ifndef MG_DEFAULT_CONFIG_FILE_NAME
#    define MG_DEFAULT_CONFIG_FILE_NAME "mg_engine.cfg"
#endif

constexpr auto k_default_config_file_name = MG_DEFAULT_CONFIG_FILE_NAME;

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

/** Size of storage for material parameter values. */
constexpr std::size_t k_material_parameters_buffer_size = 128;

// Light cluster grid configuration. The renderer divides the view frustum into a grid, each element
// of which holds a list of lights that intersect it. This allows faster light calculation.
// These are preprocessor defines so that they can be stringified and included in string literals
// (for shader code).
// See mg_light_grid.h and mg_light_buffers.h

#ifndef MG_MAX_LIGHTS_PER_CLUSTER
/** Maximum number of lights that may simultaneously affect a cluster. If more lights than this
 * number overlap a cluster, there will be artefacts. It is, however, best to keep this number
 * relatively small, to avoid growing data requirements.
 */
#    define MG_MAX_LIGHTS_PER_CLUSTER 128
#endif

#ifndef MG_MAX_NUM_LIGHTS
/** Maximum number of lights that may be rendered at a time. */
#    define MG_MAX_NUM_LIGHTS 512
#endif

#ifndef MG_LIGHT_GRID_WIDTH
/** Width of light cluster grid. */
#    define MG_LIGHT_GRID_WIDTH 16
#endif

#ifndef MG_LIGHT_GRID_HEIGHT
/** Height of light cluster grid. */
#    define MG_LIGHT_GRID_HEIGHT 8
#endif

#ifndef MG_LIGHT_GRID_DEPTH
/** Depth of light cluster grid. */
#    define MG_LIGHT_GRID_DEPTH 24
#endif

#ifndef MG_LIGHT_GRID_FAR_PLANE
/** Depth at which the light grid ends. Lights beyond this will be inside the final grid slice. */
#    define MG_LIGHT_GRID_FAR_PLANE 500
#endif

#ifndef MG_LIGHT_GRID_DEPTH_BIAS
/** Bias in depth slice calculation, used to avoid too many thin slices near the camera. */
#    define MG_LIGHT_GRID_DEPTH_BIAS -3.5f
#endif

#define MG_LIGHT_GRID_SIZE (MG_LIGHT_GRID_WIDTH * MG_LIGHT_GRID_HEIGHT * MG_LIGHT_GRID_DEPTH)

} // namespace Mg::defs
