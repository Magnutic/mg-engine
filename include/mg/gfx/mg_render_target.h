//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_render_target.h
 * Render targets, all rendering operations draw onto currently bound target.
 */

#pragma once

#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <cstdint>
#include <memory>

namespace Mg {
class Window;
}

namespace Mg::gfx {

class Texture2D;

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
    void bind() noexcept final;
    ImageSize image_size() const noexcept final { return m_image_size; }

    void update_viewport() noexcept;

private:
    WindowRenderTarget() = default;

    void set_size(int32_t width, int32_t height) noexcept { m_image_size = { width, height }; }

    ImageSize m_image_size;
};

/** Data format for colour/alpha render target. */
enum class ColourFormat {
    RGBA8 = 0x8058,   /** Red/Green/Blue/Alpha channels of 8-bit unsigned int */
    RGBA16F = 0x881A, /** Red/Green/Blue/Alpha channels of 16-bit float */
    RGBA32F = 0x8814, /** Red/Green/Blue/Alpha channels of 32-bit float */
};

struct TextureRenderTargetData;

/** Texture render targets are useful for various steps in the rendering pipeline, as they may be
 * used as texture input into later steps. A typical example is rendering to a HDR
 * TextureRenderTarget and then using the output as a texture into a post-processing step which
 * outputs to the default render target.
 */
class TextureRenderTarget : public IRenderTarget, private PImplMixin<TextureRenderTargetData> {
    struct PrivateCtorKey {};

    // TODO: support multisampling, see
    // https://www.opengl.org/wiki/Framebuffer#Multisampling_Considerations

    // TODO: support multiple render targets, i.e. more than one colour_target.
    // (probably small_vector<TextureHandle, 3>)
public:
    /** Type of depth buffer to use when a depth target texture has not been provided. */
    enum class DepthType {
        None, /** No depth buffer -- depth test will not be available when rendering to target. */
        RenderBuffer /** Generate a renderbuffer depth target: depth test will be available, but --
                       unlike when using a depth TextureTarget -- the depth buffer cannot be used as
                       a sampler later on. */
    };

    enum class BlitFilter { nearest, linear };

    struct BlitSettings {
        BlitFilter filter = BlitFilter::linear;
        bool colour = true;
        bool depth = true;
        bool stencil = true;
    };

    static void blit(const TextureRenderTarget& from,
                     const TextureRenderTarget& to,
                     const BlitSettings& settings);

    static std::unique_ptr<TextureRenderTarget>
    with_colour_target(Texture2D* colour_target, DepthType depth_type, int32_t mip_level = 0);

    static std::unique_ptr<TextureRenderTarget>
    with_colour_and_depth_targets(Texture2D* colour_target,
                                  Texture2D* depth_target,
                                  int32_t mip_level = 0);

    // Private default constructor using pass-key idiom to allow usage via make_unique.
    explicit TextureRenderTarget(PrivateCtorKey);
    ~TextureRenderTarget() override;

    MG_MAKE_NON_COPYABLE(TextureRenderTarget);
    MG_MAKE_NON_MOVABLE(TextureRenderTarget);

    void bind() final;

    ImageSize image_size() const final;

    Texture2D* colour_target() const noexcept;
    Texture2D* depth_target() const noexcept;
};

} // namespace Mg::gfx
