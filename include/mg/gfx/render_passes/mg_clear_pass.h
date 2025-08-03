//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_clear_pass.h
 */

#pragma once

#include "mg/gfx/mg_render_target.h"
#include "mg/gfx/render_passes/mg_irender_pass.h"

#include <memory>

namespace Mg::gfx {

class ClearPass : public IRenderPass {
public:
    explicit ClearPass(std::shared_ptr<IRenderTarget> target,
                       const glm::vec4 colour,
                       const bool do_clear_colour,
                       const bool do_clear_depth)
        : m_target{ std::move(target) }
        , m_colour{ colour }
        , m_do_clear_colour{ do_clear_colour }
        , m_do_clear_depth{ do_clear_depth }
    {}

    void render(const RenderParams& /*unused*/) override;

private:
    std::shared_ptr<IRenderTarget> m_target;
    glm::vec4 m_colour;
    bool m_do_clear_colour;
    bool m_do_clear_depth;
};


} // namespace Mg::gfx
