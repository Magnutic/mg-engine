//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_post_process.h
 * Post-processing functionality.
 */

#pragma once

#include "mg/gfx/mg_gfx_object_handles.h"
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
                      TextureHandle input_colour,
                      TextureHandle input_depth,
                      float z_near,
                      float z_far) noexcept;

    void drop_shaders() noexcept;
};

} // namespace Mg::gfx
