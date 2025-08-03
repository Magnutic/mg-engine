//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_scene_renderer.h
 */

#pragma once

#include "mg/core/mg_window.h"
#include "mg/resource_cache/mg_resource_cache.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

namespace Mg::gfx {

class SceneRenderer : Observer<WindowSettings> {
public:
    using RenderPasses = std::vector<std::unique_ptr<IRenderPass>>;

    explicit SceneRenderer(ResourceCache& resource_cache, Window& window)
        : m_window_settings{ window.settings() }
    {
        // Drop render passes when shader resources have changed, so changes take immediate effect.
        m_shader_file_changed_tracker = resource_cache.make_file_change_tracker(
            "ShaderResource",
            [](void* data, const FileChangedEvent&) {
                auto& self = *static_cast<SceneRenderer*>(data);
                self.m_render_passes.clear();
            },
            this);

        // Also drop when window has changed, to make new settings take effect.
        window.observe_settings(*this);
    }

    void render(const RenderParams& params)
    {
        if (m_render_passes.empty()) {
            m_render_passes = create_passes(m_window_settings);
        }

        for (auto& pass : m_render_passes) {
            pass->render(params);
        }
    }

protected:
    virtual RenderPasses create_passes(const WindowSettings& settings) = 0;

    void on_notify(const WindowSettings& settings) final
    {
        m_window_settings = settings;
        m_render_passes.clear();
    }

    std::vector<std::unique_ptr<IRenderPass>> m_render_passes;

private:
    std::shared_ptr<Mg::FileChangedTracker> m_shader_file_changed_tracker;
    WindowSettings m_window_settings;
};

} // namespace Mg::gfx
