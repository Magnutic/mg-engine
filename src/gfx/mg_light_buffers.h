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
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg::gfx {

struct Light;
class ICamera;
class LightGrid;

struct LightBuffersData;

/* Collection of GPU data structures holding information on lights. Used by renderers. */
class LightBuffers : PImplMixin<LightBuffersData> {
public:
    LightBuffers() noexcept;
    ~LightBuffers();

    MG_MAKE_DEFAULT_MOVABLE(LightBuffers);
    MG_MAKE_NON_COPYABLE(LightBuffers);

    void update(span<const Light> lights, const ICamera& cam, LightGrid& grid);

    UniformBuffer light_data_buffer;
    BufferTexture light_index_texture;
    BufferTexture tile_data_texture;
};


} // namespace Mg::gfx
