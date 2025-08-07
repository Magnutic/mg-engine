//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/render_passes/mg_clear_pass.h"

#include "../mg_opengl_loader_glad.h"

#include <algorithm>

namespace Mg::gfx {

void ClearPass::render(const RenderParams& /*unused*/)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_target->handle().as_gl_id());
    glViewport(0, 0, m_target->image_size().width, m_target->image_size().height);

    if (!m_target->is_window_render_target()) {
        const GLuint buffer = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &buffer);
    }

    // Change depth/colour write state if needed.
    std::array<GLboolean, 4> prev_colour_write = {};
    glGetBooleanv(GL_COLOR_WRITEMASK, prev_colour_write.data());
    const bool should_set_colour_write =
        m_do_clear_colour &&
        std::ranges::any_of(prev_colour_write, [](GLboolean b) { return b == GL_FALSE; });

    GLboolean prev_depth_write = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prev_depth_write);
    const bool should_set_depth_write = m_do_clear_depth && prev_depth_write == GL_FALSE;

    if (should_set_colour_write) {
        glClearColor(m_colour.w, m_colour.x, m_colour.y, m_colour.z);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    if (should_set_depth_write) {
        glDepthMask(GL_TRUE);
    }

    glClear((m_do_clear_colour ? GL_COLOR_BUFFER_BIT : 0u) |
            (m_do_clear_depth ? GL_DEPTH_BUFFER_BIT : 0u));

    // Revert depth/colour write state if it was changed.
    if (should_set_colour_write) {
        glColorMask(prev_colour_write[0],
                    prev_colour_write[1],
                    prev_colour_write[2],
                    prev_colour_write[3]);
    }
    if (should_set_depth_write) {
        glDepthMask(prev_depth_write);
    }
}

} // namespace Mg::gfx
