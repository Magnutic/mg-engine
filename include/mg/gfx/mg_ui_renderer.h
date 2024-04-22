//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_ui_renderer.h
 * 2D user-interface renderer.
 */

#pragma once

#include "mg/gfx/mg_blend_modes.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/utils/mg_angle.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"

#include <glm/vec2.hpp>

#include <string_view>

namespace Mg::gfx {

class Material;
class Texture2D;
class PreparedText;
class IRenderTarget;

/** Defines how a UI item should be placed: where on the screen and rotated in what manner. */
struct UIPlacement {
    /** Position of item to render.
     * Coordinates in [0.0, 1.0] where x = 0.0 is left of screen and y = 0.0 is bottom.
     */
    glm::vec2 position = { 0.0f, 0.0f };

    /** Offset from `position` given in units of pixels. */
    glm::vec2 position_pixel_offset = { 0.0f, 0.0f };

    /** Rotation of item to render. Measured counter-clockwise. */
    Angle rotation = 0.0_radians;

    /** Position within the item that will be at `position` and act as rotation pivot.
     * Coordinates in [0.0, 1.0] where x = 0.0 is left of item and y = 0.0 is bottom.
     */
    glm::vec2 anchor = { 0.0f, 0.0f };

    static constexpr glm::vec2 bottom_left{ 0.0f, 0.0f };
    static constexpr glm::vec2 top_left{ 0.0f, 1.0f };
    static constexpr glm::vec2 top_right{ 1.0f, 1.0f };
    static constexpr glm::vec2 bottom_right{ 1.0f, 0.0f };
    static constexpr glm::vec2 centre{ 0.5f, 0.5f };
};

/** 2D user-interface renderer. */
class UIRenderer {
public:
    explicit UIRenderer(glm::ivec2 resolution, float scaling_factor = 1.0);
    ~UIRenderer();

    MG_MAKE_NON_COPYABLE(UIRenderer);
    MG_MAKE_NON_MOVABLE(UIRenderer);

    void resolution(glm::ivec2 resolution);
    glm::ivec2 resolution() const;

    void scaling_factor(float scaling_factor);
    float scaling_factor() const;

    void draw_rectangle(const IRenderTarget& render_target,
                        const UIPlacement& placement,
                        glm::vec2 size,
                        const Material& material) noexcept;

    void draw_text(const IRenderTarget& render_target,
                   const UIPlacement& placement,
                   const PreparedText& text,
                   float scale = 1.0f,
                   BlendMode blend_mode = blend_mode_constants::bm_alpha_premultiplied) noexcept;

    void drop_shaders() noexcept;

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

} // namespace Mg::gfx
