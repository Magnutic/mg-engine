//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

#include "mg/gfx/mg_render_target.h"

#include <fmt/core.h>

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_window.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"

#include "mg_texture_node.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

namespace Mg::gfx {

//--------------------------------------------------------------------------------------------------
// WindowRenderTarget implementation
//--------------------------------------------------------------------------------------------------

void WindowRenderTarget::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    update_viewport();
}

void WindowRenderTarget::update_viewport()
{
    glViewport(0, 0, m_image_size.width, m_image_size.height);
}

//--------------------------------------------------------------------------------------------------
// TextureRenderTarget implementation
//--------------------------------------------------------------------------------------------------

namespace {

// Create a depth/stencil renderbuffer appropriate for use with the given rendertarget settings
uint32_t create_depth_stencil_buffer(ImageSize size)
{
    uint32_t id = 0;
    glGenRenderbuffers(1, &id);
    glBindRenderbuffer(GL_RENDERBUFFER, id);

    // Allocate storage
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.width, size.height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    return id;
}

void check_framebuffer()
{
    MG_CHECK_GL_ERROR();
    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        g_log.write_error(
            fmt::format("TextureRenderTarget incomplete with status code {:#x}", status));
        throw RuntimeError();
    }
}

class FramebufferBindGuard {
public:
    MG_MAKE_NON_COPYABLE(FramebufferBindGuard);
    MG_MAKE_NON_MOVABLE(FramebufferBindGuard);

    FramebufferBindGuard() { glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_binding); }
    ~FramebufferBindGuard() { glBindFramebuffer(GL_FRAMEBUFFER, uint32_t(old_binding)); }

    GLint old_binding{};
};

} // namespace

TextureRenderTarget TextureRenderTarget::with_colour_target(TextureHandle colour_target,
                                                            DepthType     depth_type)
{
    TextureRenderTarget trt;
    trt.m_colour_target = colour_target;

    const auto& texture_node = internal::texture_node(colour_target);

    // Create frame buffer object (FBO)
    GLuint fbo_id = 0;
    glGenFramebuffers(1, &fbo_id);

    FramebufferBindGuard fbg;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);

    // Attach texture to FBO
    auto gl_tex_id = static_cast<GLuint>(texture_node.texture.gfx_api_handle());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_tex_id, 0);

    // Attach depth/stencil renderbuffer to FBO
    switch (depth_type) {
    case DepthType::RenderBuffer:
        trt.m_depth_buffer_id = create_depth_stencil_buffer(texture_node.texture.image_size());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  static_cast<uint32_t>(trt.m_depth_buffer_id.value));
        break;
    case DepthType::None:
        break; // Do nothing
    }

    trt.m_fbo_id = fbo_id;
    check_framebuffer();

    return trt;
}

TextureRenderTarget TextureRenderTarget::with_colour_and_depth_targets(TextureHandle colour_target,
                                                                       TextureHandle depth_target)
{
    MG_ASSERT(colour_target != depth_target);
    const auto& colour_tex = internal::texture_node(colour_target).texture;
    const auto& depth_tex  = internal::texture_node(depth_target).texture;

    if (colour_tex.image_size() != depth_tex.image_size()) {
        g_log.write_warning(
            "TextureRenderTarget::with_colour_and_depth_targets(): colour_target and depth_target "
            "have different image sizes.");
        g_log.write_verbose(
            fmt::format("\n\tColour target '{}': {}x{}\n\tDepth target '{}': {}x{}.",
                        colour_tex.id().c_str(),
                        colour_tex.image_size().width,
                        colour_tex.image_size().height,
                        depth_tex.id().c_str(),
                        depth_tex.image_size().width,
                        depth_tex.image_size().height));
    }

    TextureRenderTarget trt;
    trt.m_colour_target = colour_target;
    trt.m_depth_target  = depth_target;

    // Create frame buffer object (FBO)
    GLuint fbo_id = 0;
    glGenFramebuffers(1, &fbo_id);

    FramebufferBindGuard fbg;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);

    // Attach texture to FBO
    auto colour_id = static_cast<GLuint>(colour_tex.gfx_api_handle());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour_id, 0);

    // Attach depth/stencil renderbuffer to FBO
    auto depth_id = static_cast<GLuint>(depth_tex.gfx_api_handle());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth_id, 0);

    trt.m_fbo_id = fbo_id;
    check_framebuffer();

    return trt;
}

TextureRenderTarget::~TextureRenderTarget()
{
    // If this buffer is bound, revert to default render target
    GLint current_binding;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_binding);

    if (current_binding == static_cast<GLint>(m_fbo_id.value)) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Delete OpenGL objects
    GLuint fbo_id = static_cast<GLuint>(m_fbo_id.value);
    glDeleteFramebuffers(1, &fbo_id);
}

void TextureRenderTarget::bind()
{
    MG_ASSERT(m_fbo_id != 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_fbo_id.value));
    uint32_t buffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buffer);

    // Set up viewport to match FBO size
    glViewport(0, 0, image_size().width, image_size().height);
}

ImageSize TextureRenderTarget::image_size() const
{
    return internal::texture_node(m_colour_target).texture.image_size();
}

} // namespace Mg::gfx
