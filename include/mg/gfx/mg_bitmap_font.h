//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_bitmap_font.h
 * Loads and rasterises fonts, and prepares texts using the rasterised font that can be drawn using
 * Mg::gfx::UIRenderer.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <string_view>

namespace Mg {
class FontResource;
struct UnicodeRange;
} // namespace Mg

namespace Mg::gfx {

/** Parameters controlling the typesetting of text. */
struct TypeSetting {
    /** Vertical space between lines of text as factor of line height. */
    float line_spacing_factor = 1.5f;

    /** Maximum width of a line of text in pixels before line break. Optional, if left
     * undefined, there will be no automatic line breaks.
     */
    Opt<int32_t> max_width_pixels;
};

class PreparedText {
public:
    struct GpuData {
        TextureHandle texture;
        BufferHandle vertex_buffer;
        VertexArrayHandle vertex_array;
    };

    ~PreparedText();

    MG_MAKE_DEFAULT_MOVABLE(PreparedText);
    MG_MAKE_NON_COPYABLE(PreparedText);

    const GpuData& gpu_data() const { return m_gpu_data; }

    float width() const { return m_width; }
    float height() const { return m_height; }

    size_t num_glyphs() const { return m_num_glyphs; }

private:
    friend class BitmapFont;

    explicit PreparedText(GpuData gpu_data, float width, float height, size_t num_glyphs)
        : m_gpu_data(std::move(gpu_data))
        , m_width(width)
        , m_height(height)
        , m_num_glyphs(num_glyphs)
    {}

    // GPU buffer handles: texture and vertex buffers for this text.
    GpuData m_gpu_data;

    // Dimensions of text in pixels.
    float m_width;
    float m_height;

    // Number of glyphs (i.e. number of rectangles to draw).
    size_t m_num_glyphs;
};

struct BitmapFontData;

class BitmapFont : PImplMixin<BitmapFontData> {
public:
    explicit BitmapFont(ResourceHandle<FontResource> font,
                        int font_size_pixels,
                        span<const UnicodeRange> unicode_ranges);

    ~BitmapFont();

    MG_MAKE_NON_COPYABLE(BitmapFont);
    MG_MAKE_NON_MOVABLE(BitmapFont);

    PreparedText prepare_text(std::string_view text_utf8, const TypeSetting& typesetting) const;

    span<const UnicodeRange> contained_ranges() const;

    int font_size_pixels() const;
};

} // namespace Mg::gfx
