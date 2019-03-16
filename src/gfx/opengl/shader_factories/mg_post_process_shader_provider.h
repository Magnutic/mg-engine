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

/** @file mg_post_process_shader_provider.h
 * Creates shader programs for PostProcessRenderer.
 */

#pragma once

#include "mg/gfx/mg_pipeline_repository.h"

namespace Mg::gfx::post_renderer {

// Texture units 8 & 9 are reserved for input colour and depth texture, respectively.
static constexpr uint32_t k_input_colour_texture_unit = 8;
static constexpr uint32_t k_input_depth_texture_unit  = 9;

static constexpr uint32_t k_material_params_ubo_slot = 0;
static constexpr uint32_t k_frame_block_ubo_slot     = 1;

struct FrameBlock {
    float z_near;
    float z_far;
};

} // namespace Mg::gfx::post_renderer

namespace Mg::gfx {

experimental::PipelineRepository make_post_process_pipeline_repository();

} // namespace Mg::gfx
