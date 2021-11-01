//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_light_grid.h
 * Divides the view space frustum into a grid of tiles. Used for the Tiled Rendering and Clustered
 * Rendering techniques.
 */

#pragma once

#include "mg/mg_defs.h"

namespace Mg::gfx {

/** Parameters controlling the light grid for clustered rendering. */
struct LightGridConfig {
    /** Size of array of light sources. */
    std::size_t max_num_lights = defs::k_default_max_num_lights;

    /** Maximum number of lights that may simultaneously affect a cluster. If more lights than this
     * number overlap a cluster, there will be artefacts. It is, however, best to keep this number
     * relatively small, to avoid growing data requirements.
     */
    std::size_t max_lights_per_cluster = defs::k_default_max_lights_per_cluster;

    /** Width of the light cluster grid. */
    std::size_t grid_width = defs::k_default_light_grid_width;

    /** Height of the light cluster grid. */
    std::size_t grid_height = defs::k_default_light_grid_height;

    /** Depth of the light cluster grid. */
    std::size_t grid_depth = defs::k_default_light_grid_depth;

    /** Depth at which the light grid ends. Lights beyond this will be inside the final grid slice.
     */
    std::size_t grid_far_plane = defs::k_default_light_grid_far_plane;

    /** Bias in depth slice calculation, used to avoid too many thin slices near the camera. */
    float depth_bias = defs::k_default_light_grid_depth_bias;
};

} // namespace Mg::gfx
