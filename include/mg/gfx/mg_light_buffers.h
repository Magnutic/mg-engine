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

void update_light_data(LightBuffers&     light_data_out,
                       span<const Light> lights,
                       const ICamera&    cam,
                       LightGrid&        grid);


} // namespace Mg::gfx
