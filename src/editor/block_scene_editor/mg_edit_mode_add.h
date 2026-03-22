//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2026, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_edit_mode_add.h
 * Edit mode for adding blocks to a block scene.
 */

#pragma once

#include "mg_edit_mode_render_common.h"

#include "mg/scene/mg_block_scene.h"
#include "mg_iedit_mode.h"

#include <glm/vec4.hpp>

namespace Mg {

/** Edit mode for adding blocks to a block scene. */
class EditModeAdd : public IEditMode {
public:
    void render(const gfx::ICamera& cam,
                const BlockScene& scene,
                gfx::IRenderTarget& render_target,
                EditModeRenderCommon& render_common) override
    {
        const auto colour = can_insert ? glm::vec4(0.25f, 1.0f, 0.25f, 1.0f)
                                       : glm::vec4(1.0f, 0.25f, 0.25f, 1.0f);

        if (m_coord.has_value()) {
            render_common.draw_grid_around_point(cam,
                                                 render_target,
                                                 *m_coord,
                                                 m_new_block_z_min,
                                                 scene.block_size());

            render_common.draw_box_outline(cam,
                                           render_target,
                                           m_coord->x,
                                           m_coord->y,
                                           m_new_block_z_min,
                                           m_new_block_z_max,
                                           scene.block_size(),
                                           colour);
        }
    }

    bool update(BlockScene& scene, const UpdateArgs& data) override
    {
        const bool modify = data.button_states.get("modify").is_held;

        const auto block_size = scene.block_size();

        if (std::abs(data.view_angle.z) < FLT_EPSILON) {
            return false;
        }

        if (data.mouse_scroll_delta.y >= 1.0f) {
            m_new_block_z_max += k_vertical_step;
            if (!modify && m_new_block_z_min + k_vertical_step < m_new_block_z_max) {
                m_new_block_z_min += k_vertical_step;
            }
        }
        else if (data.mouse_scroll_delta.y <= -1.0f) {
            if (!modify) {
                m_new_block_z_min -= k_vertical_step;
            }
            if (m_new_block_z_min + k_vertical_step < m_new_block_z_max) {
                m_new_block_z_max -= k_vertical_step;
            }
        }

        m_coord = nullopt;
        m_cursor_target = get_target_position(data);
        if (m_cursor_target) {
            m_coord = { narrow_cast<int>(std::floor(m_cursor_target->x / block_size)),
                        narrow_cast<int>(std::floor(m_cursor_target->y / block_size)) };
        }

        auto existing_block_overlaps = [&] {
            std::vector<Block> blocks;
            scene.get_blocks_at(m_coord->x, m_coord->y, blocks);
            return std::ranges::any_of(blocks, [&](const auto& b) {
                return b.z_min < m_new_block_z_max && b.z_max > m_new_block_z_min;
            });
        };

        can_insert = m_coord.has_value() && !existing_block_overlaps();

        if (can_insert && data.button_states.get("interact").was_pressed) {
            return scene.try_insert(m_coord->x,
                                    m_coord->y,
                                    { .z_min = m_new_block_z_min, .z_max = m_new_block_z_max });
        }

        return false;
    }

    std::string name() const override { return "Add"; }

    Opt<glm::vec3> get_target_position(const UpdateArgs& data) const
    {
        if (data.view_angle.z == 0.0f) {
            return nullopt;
        }

        const float distance = (m_new_block_z_min - data.view_position.z) / data.view_angle.z;
        if (distance <= 0.0f) {
            return nullopt;
        }

        return data.view_position + data.view_angle * distance;
    }

private:
    float m_new_block_z_min = 0.0f;
    float m_new_block_z_max = 1.0f;
    Opt<glm::vec3> m_cursor_target;
    Opt<glm::ivec2> m_coord;
    bool can_insert = false;
};

} // namespace Mg
