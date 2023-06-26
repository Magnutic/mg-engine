//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_light_buffers.h
 * Collection of GPU data structures holding information on lights, used by renderers.
 */

#pragma once

#include "mg/gfx/mg_buffer_texture.h"
#include "mg/gfx/mg_uniform_buffer.h"
#include "mg/utils/mg_impl_ptr.h"

namespace Mg::gfx {

struct Light;
struct LightGridConfig;
class ICamera;
class LightGrid;

/* Collection of GPU data structures holding information on lights. Used by renderers. */
class LightBuffers {
public:
    explicit LightBuffers(const LightGridConfig& grid_config);

    void update(std::span<const Light> lights, const ICamera& cam);

    const LightGridConfig& config() const;

    LightGrid& grid();

    UniformBuffer light_block_buffer;
    BufferTexture light_index_texture;
    BufferTexture clusters_texture;

private:
    struct Impl;
    ImplPtr<Impl> m_impl;
};


} // namespace Mg::gfx
