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

/** @file mg_render_target.h
 * Render targets, all rendering operations draw onto currently bound target.
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>

#include <mg/gfx/mg_texture2d.h>
#include <mg/gfx/mg_texture_handle.h>
#include <mg/gfx/mg_texture_related_types.h>
#include <mg/utils/mg_macros.h>
#include <mg/utils/mg_object_id.h>

namespace Mg {
class Window;
}

namespace Mg::gfx {

class IRenderTarget {
public:
    MG_INTERFACE_BOILERPLATE(IRenderTarget);

    virtual void bind() = 0;

    virtual ImageSize image_size() const = 0;
};

/** The default render target is the final output to window. Mg::Window owns an instance of this. */
class WindowRenderTarget : public IRenderTarget {
    friend class ::Mg::Window;

public:
    void      bind() final;
    ImageSize image_size() const noexcept final { return m_image_size; }

    void update_viewport();

    MG_MAKE_NON_COPYABLE(WindowRenderTarget);
    MG_MAKE_NON_MOVABLE(WindowRenderTarget);

private:
    WindowRenderTarget() {}

    void set_size(int32_t width, int32_t height) { m_image_size = { width, height }; }

    ImageSize m_image_size;
};

/** Data format for colour/alpha render target. */
enum class ColourFormat {
    RGBA8   = 0x8058, /** Red/Green/Blue/Alpha channels of 8-bit unsigned int */
    RGBA16F = 0x881A, /** Red/Green/Blue/Alpha channels of 16-bit float */
    RGBA32F = 0x8814, /** Red/Green/Blue/Alpha channels of 32-bit float */
};

/** Texture render targets are useful for various steps in the rendering pipeline, as they may be
 * used as texture input into later steps. A typical example is rendering to a HDR
 * TextureRenderTarget and then using the output as a texture into a post-processing step which
 * outputs to the default render target.
 */
class TextureRenderTarget : public IRenderTarget {
    // TODO: support multisampling, see
    // https://www.opengl.org/wiki/Framebuffer#Multisampling_Considerations

    // TODO: support multiple render targets, i.e. more than one colour_target.
    // (probably small_vector<std::shared_ptr<TargetTexture>, 3>)
public:
    /** Type of depth buffer to use when a depth target texture has not been provided. */
    enum class DepthType {
        None, /** No depth buffer -- depth test will not be available when rendering to target. */
        RenderBuffer /** Generate a renderbuffer depth target: depth test will be available, but --
                       unlike when using a depth TextureTarget -- the depth buffer cannot be used as
                       a sampler later on. */
    };

    static TextureRenderTarget with_colour_target(TextureHandle colour_target,
                                                  DepthType     depth_type);

    static TextureRenderTarget with_colour_and_depth_targets(TextureHandle colour_target,
                                                             TextureHandle depth_target);

    MG_MAKE_DEFAULT_MOVABLE(TextureRenderTarget);
    MG_MAKE_NON_COPYABLE(TextureRenderTarget);
    ~TextureRenderTarget();

    void bind() final;

    ImageSize image_size() const final;

    const Texture2D& colour_target() const;

    const Texture2D& depth_target() const;

private:
    TextureRenderTarget() = default;

    TextureHandle m_colour_target{};
    TextureHandle m_depth_target{};

    ObjectId m_depth_buffer_id{ 0 }; // Depth renderbuffer which may be used if
                                     // depth target texture is not present.
    ObjectId m_fbo_id{ 0 };
};

} // namespace Mg::gfx
