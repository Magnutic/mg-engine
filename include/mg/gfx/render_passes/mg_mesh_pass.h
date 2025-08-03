
//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_pass.h
 */

#pragma once

#include "mg/gfx/mg_light.h"
#include "mg/gfx/mg_mesh_renderer.h"
#include "mg/gfx/mg_render_command_list.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

struct SceneLights {
    std::vector<Light> point_lights;
};

class MeshPass : public IRenderPass {
public:
    explicit MeshPass(std::shared_ptr<IRenderTarget> target,
                      std::shared_ptr<SceneLights> scene_lights,
                      std::shared_ptr<RenderCommandProducer> render_command_producer,
                      SortingMode sorting_mode = SortingMode::near_to_far)
        : m_renderer{ LightGridConfig{} }
        , m_target{ std::move(target) }
        , m_scene_lights{ std::move(scene_lights) }
        , m_render_command_producer{ std::move(render_command_producer) }
        , m_sorting_mode{ sorting_mode }
    {}

    void render(const RenderParams& params) override
    {
        const auto& commands = m_render_command_producer->finalize(params.camera, m_sorting_mode);
        m_renderer.render(params.camera,
                          commands,
                          m_scene_lights->point_lights,
                          *m_target,
                          { params.time_since_init });
    }

private:
    MeshRenderer m_renderer;
    std::shared_ptr<IRenderTarget> m_target;
    std::shared_ptr<SceneLights> m_scene_lights;
    std::shared_ptr<RenderCommandProducer> m_render_command_producer;
    SortingMode m_sorting_mode;
};

} // namespace Mg::gfx
