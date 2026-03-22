//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2026, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_edit_mode_edit.h
 * Edit mode for adjusting blocks in a block scene.
 */

#pragma once

#include "mg_edit_mode_render_common.h"
#include "mg_iedit_mode.h"
#include "mg_try_blocks_on_grid.h"

#include "mg/scene/mg_block_scene.h"
#include "mg/utils/mg_math_utils.h"

#include <glm/vec4.hpp>

namespace Mg {

/** Edit mode for adjusting blocks in a block scene. */
class EditModeEdit : public IEditMode {
public:
    void render(const gfx::ICamera& cam,
                const BlockScene& scene,
                gfx::IRenderTarget& render_target,
                EditModeRenderCommon& render_common) override
    {
        const auto colour = glm::vec4(0.25f, 0.25f, 1.0f, 1.0f);
        if (m_selected_block.has_value()) {
            render_common.draw_box_outline(cam,
                                           render_target,
                                           m_selected_block->x,
                                           m_selected_block->y,
                                           m_selected_block->z_min,
                                           m_selected_block->z_max,
                                           scene.block_size(),
                                           colour);
        }
    }

    bool update(BlockScene& scene, const UpdateArgs& data) override
    {
        m_selected_block = nullopt;
        std::vector<Block> blocks;

        auto vertically_overlaps_block = [](const float z_min, const float z_max) {
            return [z_min, z_max](const Block& b) { return b.z_max >= z_min && b.z_min <= z_max; };
        };

        auto vertical_distance_to_block = [](const float z) {
            return [z](const Block& b) {
                const auto is_within = z >= b.z_min && z <= b.z_max;
                return is_within ? 0.0f : min(abs(b.z_min - z), abs(b.z_max - z));
            };
        };

        try_blocks_on_grid(
            data.view_position,
            data.view_angle,
            [&](int x, int y, float z_entry, float z_exit) {
                blocks.clear();
                scene.get_blocks_at(x, y, blocks);
                std::erase_if(blocks,
                              std::not_fn(vertically_overlaps_block(min(z_entry, z_exit),
                                                                    max(z_entry, z_exit))));
                if (blocks.empty()) {
                    return false;
                }

                // If several blocks in the cell overlap the vertical range of the ray within the
                // cell, pick the one that's closest to the ray's entrypoint into the cell.
                if (blocks.size() > 1) {
                    std::ranges::partial_sort(blocks,
                                              std::next(blocks.begin()),
                                              std::less{},
                                              vertical_distance_to_block(z_entry));
                }

                m_selected_block = SelectedBlock{ x, y, blocks[0].z_min, blocks[0].z_max };
                return true;
            },
            scene.block_size(),
            500.0f);

        if (!m_selected_block) {
            return false;
        }

        const auto [selected_block_level, selected_block_data] = [&] {
            blocks.clear();
            scene.get_blocks_at(m_selected_block->x, m_selected_block->y, blocks);
            for (size_t i = 0; i < blocks.size(); ++i) {
                if (blocks[i].z_min == m_selected_block->z_min) {
                    return std::pair{ i, blocks[i] };
                }
            }

            MG_ASSERT(false && "unreachable");
        }();

        const bool modify = data.button_states.get("modify").is_held;

        auto new_block_data = selected_block_data;

        if (data.mouse_scroll_delta.y >= 1.0f) {
            new_block_data.z_max += k_vertical_step;
            if (!modify && new_block_data.z_min + k_vertical_step < new_block_data.z_max) {
                new_block_data.z_min += k_vertical_step;
            }
        }
        else if (data.mouse_scroll_delta.y <= -1.0f) {
            if (!modify) {
                new_block_data.z_min -= k_vertical_step;
            }
            if (new_block_data.z_min + k_vertical_step < new_block_data.z_max) {
                new_block_data.z_max -= k_vertical_step;
            }
        }

        if (new_block_data != selected_block_data) {
            scene.try_delete(m_selected_block->x, m_selected_block->y, selected_block_level);
            const bool success =
                scene.try_insert(m_selected_block->x, m_selected_block->y, new_block_data);

            if (!success) {
                // Restore original block.
                scene.try_insert(m_selected_block->x, m_selected_block->y, selected_block_data);
            }

            return success;
        }

        if (data.button_states.get("interact_2").was_pressed) {
            Opt<BlockTextures&> textures = scene.block_textures_at(m_selected_block->x,
                                                                   m_selected_block->y,
                                                                   selected_block_level);
            textures.map([](auto& t) {
                t[BlockFace::top] = (t[BlockFace::top] + 1) % k_block_scene_max_num_textures;
            });
            return textures.has_value();
        }

        if (data.button_states.get("delete").was_pressed) {
            return scene.try_delete(m_selected_block->x, m_selected_block->y, selected_block_level);
        }

        return false;
    }

    std::string name() const override { return "Edit"; }

    Opt<SelectedBlock> m_selected_block;
};


} // namespace Mg
