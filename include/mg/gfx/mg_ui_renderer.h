//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_billboard_renderer.h
 * 2D user-interface component renderer.
 */

#pragma once

#include <mg/gfx/mg_blend_modes.h>
#include <mg/utils/mg_angle.h>
#include <mg/utils/mg_macros.h>
#include <mg/utils/mg_optional.h>
#include <mg/utils/mg_simple_pimpl.h>

#include <glm/vec2.hpp>

#include <cmath>

namespace Mg::gfx {

class Material;
class Texture2D;

struct UIRendererData;

class UIRenderer : PImplMixin<UIRendererData> {
public:
    UIRenderer();
    ~UIRenderer();

    MG_MAKE_NON_COPYABLE(UIRenderer);
    MG_MAKE_NON_MOVABLE(UIRenderer);

    struct TransformParams {
        /** Position of item to render.
         * Coordinates in [0.0, 1.0] where x = 0.0 is left of screen and y = 0.0 is bottom.
         */
        glm::vec2 position = { 0.0f, 0.0f };

        /** Size of item to render given as proportion of screen height. */
        glm::vec2 size = { 1.0f, 1.0f };

        Angle rotation = 0.0_radians;

        /** Position within the item that will be at `position` and act as rotation pivot.
         * Coordinates in [0.0, 1.0] where x = 0.0 is left of item and y = 0.0 is bottom.
         */
        glm::vec2 anchor = { 0.0f, 0.0f };
    };

    void aspect_ratio(float aspect_ratio);
    float aspect_ratio() const;

    void draw_rectangle(const TransformParams& params,
                        const Material* material,
                        Opt<BlendMode> blend_mode) noexcept;

    void drop_shaders() noexcept;
};

} // namespace Mg::gfx
