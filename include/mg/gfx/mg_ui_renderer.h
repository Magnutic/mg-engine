//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_ui_renderer.h
 * 2D user-interface renderer.
 */

#pragma once

#include <mg/gfx/mg_blend_modes.h>
#include <mg/gfx/mg_gfx_object_handles.h>
#include <mg/utils/mg_angle.h>
#include <mg/utils/mg_macros.h>
#include <mg/utils/mg_optional.h>
#include <mg/utils/mg_simple_pimpl.h>

#include <glm/vec2.hpp>

#include <string_view>

namespace Mg::gfx {

class Material;
class Texture2D;
class PreparedText;
class FontHandler;

namespace detail {
struct UIRendererData;
}

/** 2D user-interface renderer. */
class UIRenderer : PImplMixin<detail::UIRendererData> {
public:
    explicit UIRenderer(glm::ivec2 resolution, float scaling_factor = 1.0);
    ~UIRenderer();

    MG_MAKE_NON_COPYABLE(UIRenderer);
    MG_MAKE_NON_MOVABLE(UIRenderer);

    struct TransformParams {
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
    };

    void resolution(glm::ivec2 resolution);
    glm::ivec2 resolution() const;

    void scaling_factor(float scaling_factor);
    float scaling_factor() const;

    FontHandler& font_handler();
    const FontHandler& font_handler() const;

    void draw_rectangle(glm::vec2 size,
                        const TransformParams& transform_params,
                        const Material& material,
                        Opt<BlendMode> blend_mode) noexcept;

    void draw_text(const PreparedText& text,
                   const TransformParams& transform_params,
                   float scale = 1.0f,
                   BlendMode blend_mode = blend_mode_constants::bm_alpha_premultiplied) noexcept;

    void drop_shaders() noexcept;
};

} // namespace Mg::gfx
