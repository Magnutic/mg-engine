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

/** @file mg_post_process.h
 * Post-processing functionality.
 */

#pragma once

#include "mg/gfx/mg_texture_handle.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg::gfx {

class Material;

struct PostProcessRendererData;

class PostProcessRenderer : PImplMixin<PostProcessRendererData> {
public:
    explicit PostProcessRenderer();

    MG_MAKE_NON_COPYABLE(PostProcessRenderer);
    MG_MAKE_DEFAULT_MOVABLE(PostProcessRenderer);

    ~PostProcessRenderer();

    /** Post-process using only colour texture as input. */
    void post_process(const Material& material, TextureHandle input_colour) noexcept;

    /** Post-process using colour and depth textures. */
    void post_process(const Material& material,
                      TextureHandle   input_colour,
                      TextureHandle   input_depth,
                      float           z_near,
                      float           z_far) noexcept;

    void drop_shaders() noexcept;
};

} // namespace Mg::gfx
