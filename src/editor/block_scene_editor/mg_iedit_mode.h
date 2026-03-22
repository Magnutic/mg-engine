//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2026, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_iedit_mode.h
 * .
 */

#pragma once

#include "mg/core/input/mg_input.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Mg {

class BlockScene;
class EditModeRenderCommon;

namespace gfx {
class ICamera;
class IRenderTarget;
} // namespace gfx

class IEditMode {
public:
    static constexpr float k_vertical_step = 0.25f;

    struct UpdateArgs {
        glm::vec3 view_position;
        glm::vec3 view_angle;
        const input::ButtonTracker::ButtonStates& button_states;
        glm::vec2 mouse_scroll_delta;
    };

    struct SelectedBlock {
        int x;
        int y;
        float z_min;
        float z_max;
    };

    MG_INTERFACE_BOILERPLATE(IEditMode);

    virtual void render(const gfx::ICamera& cam,
                        const BlockScene& scene,
                        gfx::IRenderTarget& render_target,
                        EditModeRenderCommon& render_common) = 0;

    virtual bool update(BlockScene& scene, const UpdateArgs& data) = 0;

    virtual std::string name() const = 0;
};


} // namespace Mg
