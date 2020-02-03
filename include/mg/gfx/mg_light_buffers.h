//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_light_buffers.h
 * Collection of GPU data structures holding information on lights, used by renderers.
 */

#pragma once

#include "mg/gfx/mg_buffer_texture.h"
#include "mg/gfx/mg_light.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/mg_defs.h"

namespace Mg::gfx {

class ICamera;
class LightGrid;

/* Collection of GPU data structures holding information on lights, used by renderers. */
struct LightBuffers {
    explicit LightBuffers() noexcept;

    UniformBuffer light_data_buffer;
    BufferTexture light_index_texture;
    BufferTexture tile_data_texture;
};

void update_light_data(LightBuffers& light_data_out,
                       span<const Light> lights,
                       const ICamera& cam,
                       LightGrid& grid);


} // namespace Mg::gfx
