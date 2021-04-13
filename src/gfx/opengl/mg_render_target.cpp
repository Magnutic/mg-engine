//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_render_target.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_window.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_texture2d.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_gsl.h"

#include "mg_gl_debug.h"
#include "mg_glad.h"

#include <fmt/core.h>

namespace Mg::gfx {

//--------------------------------------------------------------------------------------------------
// WindowRenderTarget implementation
//--------------------------------------------------------------------------------------------------

void WindowRenderTarget::bind() noexcept
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    update_viewport();
}

void WindowRenderTarget::update_viewport() noexcept
{
    glViewport(0, 0, m_image_size.width, m_image_size.height);
}

//--------------------------------------------------------------------------------------------------
// TextureRenderTarget implementation
//--------------------------------------------------------------------------------------------------

namespace {

// Create a depth/stencil renderbuffer appropriate for use with the given rendertarget settings
uint32_t create_depth_stencil_buffer(ImageSize size) noexcept
{
    MG_GFX_DEBUG_GROUP("create_depth_stencil_buffer")
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
    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        log.error(
            fmt::format("TextureRenderTarget incomplete with status code {:#x}", status));
        throw RuntimeError();
    }
}

class FramebufferBindGuard {
public:
    MG_MAKE_NON_COPYABLE(FramebufferBindGuard);
    MG_MAKE_NON_MOVABLE(FramebufferBindGuard);

    FramebufferBindGuard() noexcept
    {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read_binding);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_write_binding);
    }
    ~FramebufferBindGuard()
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, narrow<uint32_t>(old_read_binding));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, narrow<uint32_t>(old_write_binding));
    }

    GLint old_read_binding{};
    GLint old_write_binding{};
};

} // namespace

struct TextureRenderTargetData {
    Texture2D* colour_target{};
    Texture2D* depth_target{};

    TextureHandle depth_buffer_id; // Depth renderbuffer which may be used if
                                   // depth target texture is not present.
    GLuint fbo_id = 0;

    int32_t mip_level = 0;
};

void TextureRenderTarget::blit(const TextureRenderTarget& from,
                               const TextureRenderTarget& to,
                               const BlitSettings& settings)
{
    MG_GFX_DEBUG_GROUP("TextureRenderTarget::with_colour_target")
    FramebufferBindGuard guard;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, from.impl().fbo_id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, to.impl().fbo_id);

    const GLuint flags = (settings.colour ? GL_COLOR_BUFFER_BIT : 0u) |
                         (settings.depth ? GL_DEPTH_BUFFER_BIT : 0u) |
                         (settings.stencil ? GL_STENCIL_BUFFER_BIT : 0u);

    const GLenum filter = (settings.filter == BlitFilter::linear) ? GL_LINEAR : GL_NEAREST;

    const auto [from_width, from_height] = from.image_size();
    const auto [to_width, to_height] = to.image_size();

    glBlitFramebuffer(0, 0, from_width, from_height, 0, 0, to_width, to_height, flags, filter);
}

std::unique_ptr<TextureRenderTarget>
TextureRenderTarget::with_colour_target(Texture2D* colour_target,
                                        DepthType depth_type,
                                        int32_t mip_level)
{
    MG_GFX_DEBUG_GROUP("TextureRenderTarget::with_colour_target")

    auto trt = std::make_unique<TextureRenderTarget>(PrivateCtorKey{});
    TextureRenderTargetData& data = trt->impl();

    data.colour_target = colour_target;
    data.mip_level = mip_level;

    // Create frame buffer object (FBO)
    glGenFramebuffers(1, &data.fbo_id);

    FramebufferBindGuard fbg;
    glBindFramebuffer(GL_FRAMEBUFFER, data.fbo_id);

    // Attach texture to FBO
    const auto gl_tex_id = colour_target->handle().as_gl_id();
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           gl_tex_id,
                           mip_level);

    // Attach depth/stencil renderbuffer to FBO
    switch (depth_type) {
    case DepthType::RenderBuffer: {
        const GLuint depth_buffer_id = create_depth_stencil_buffer(colour_target->image_size());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  depth_buffer_id);
        data.depth_buffer_id.set(depth_buffer_id);
        break;
    }
    case DepthType::None:
        break; // Do nothing
    }

    check_framebuffer();

    return trt;
}

std::unique_ptr<TextureRenderTarget>
TextureRenderTarget::with_colour_and_depth_targets(Texture2D* colour_target,
                                                   Texture2D* depth_target,
                                                   int32_t mip_level)
{
    MG_GFX_DEBUG_GROUP("TextureRenderTarget::with_colour_and_depth_targets")

    MG_ASSERT(colour_target != depth_target);

    if (colour_target->image_size() != depth_target->image_size()) {
        log.warning(
            "TextureRenderTarget::with_colour_and_depth_targets(): colour_target and depth_target "
            "have different image sizes.");
        log.verbose(
            fmt::format("\n\tColour target '{}': {}x{}\n\tDepth target '{}': {}x{}.",
                        colour_target->id().c_str(),
                        colour_target->image_size().width,
                        colour_target->image_size().height,
                        depth_target->id().c_str(),
                        depth_target->image_size().width,
                        depth_target->image_size().height));
    }

    auto trt = std::make_unique<TextureRenderTarget>(PrivateCtorKey{});
    TextureRenderTargetData& data = trt->impl();
    data.colour_target = colour_target;
    data.depth_target = depth_target;
    data.mip_level = mip_level;

    // Create frame buffer object (FBO)
    glGenFramebuffers(1, &data.fbo_id);

    FramebufferBindGuard fbg;
    glBindFramebuffer(GL_FRAMEBUFFER, data.fbo_id);

    // Attach texture to FBO
    const auto colour_id = colour_target->handle().as_gl_id();
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           colour_id,
                           mip_level);

    // Attach depth/stencil renderbuffer to FBO
    const auto depth_id = depth_target->handle().as_gl_id();
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D,
                           depth_id,
                           mip_level);

    check_framebuffer();

    return trt;
}

TextureRenderTarget::TextureRenderTarget(PrivateCtorKey) {}

TextureRenderTarget::~TextureRenderTarget()
{
    MG_GFX_DEBUG_GROUP("TextureRenderTarget::~TextureRenderTarget")

    // If this buffer is bound, revert to default render target
    GLint current_binding;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_binding);

    if (static_cast<GLuint>(current_binding) == impl().fbo_id) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Delete OpenGL objects
    glDeleteFramebuffers(1, &impl().fbo_id);
}

void TextureRenderTarget::bind()
{
    MG_GFX_DEBUG_GROUP("TextureRenderTarget::bind")

    MG_ASSERT(impl().fbo_id != 0);
    glBindFramebuffer(GL_FRAMEBUFFER, impl().fbo_id);
    const GLuint buffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &buffer);

    // Set up viewport to match FBO size
    glViewport(0, 0, image_size().width, image_size().height);
}

ImageSize TextureRenderTarget::image_size() const
{
    ImageSize size = impl().colour_target->image_size();
    size.width >>= impl().mip_level;
    size.height >>= impl().mip_level;
    return size;
}

Texture2D* TextureRenderTarget::colour_target() const noexcept
{
    return impl().colour_target;
}

Texture2D* TextureRenderTarget::depth_target() const noexcept
{
    return impl().depth_target;
}

} // namespace Mg::gfx
