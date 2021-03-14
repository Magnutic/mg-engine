//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_font_handler.h
 * Loads and rasterises fonts and prepares texts that can be draw using Mg::gfx::UIRenderer.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/mg_unicode.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <string>
#include <string_view>

namespace Mg {
class FontResource;
}

namespace Mg::gfx {

/** Identifies a particular font and size. */
class FontId {
public:
    FontId() = default;

    FontId(ResourceHandle<FontResource> resource, const int pixel_size)
        : m_resource(resource), m_pixel_size(pixel_size)
    {}

    friend bool operator==(const FontId& l, const FontId& r) noexcept
    {
        return l.name() == r.name() && l.m_pixel_size == r.m_pixel_size;
    }

    friend bool operator!=(const FontId& l, const FontId& r) noexcept { return !(l == r); }

    Identifier name() const noexcept { return m_resource.resource_id(); }
    ResourceHandle<FontResource> resource() const noexcept { return m_resource; }
    int pixel_size() const noexcept { return m_pixel_size; }

    // Comparator for `FontId`s so that they can be used in a map.
    struct Cmp {
        bool operator()(const FontId& l, const FontId& r)
        {
            return Identifier::HashCompare{}(l.name(), r.name()) ||
                   (l.name() == r.name() && l.pixel_size() < r.pixel_size());
        }
    };

private:
    ResourceHandle<FontResource> m_resource;
    int m_pixel_size = 0;
};

// TODO rename DrawableText?
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
    friend class FontHandler;

    PreparedText(GpuData gpu_data, float width, float height, size_t num_glyphs)
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

struct FontHandlerData;

/* Loads and rasterises fonts and prepares texts that can be drawn using Mg::gfx::UIRenderer. */
class FontHandler : PImplMixin<FontHandlerData> {
public:
    explicit FontHandler();
    ~FontHandler();

    MG_MAKE_NON_COPYABLE(FontHandler);
    MG_MAKE_NON_MOVABLE(FontHandler);

    FontId load_font(ResourceHandle<FontResource> font,
                     int32_t pixel_size,
                     span<const UnicodeRange> unicode_ranges);

    /** Parameters controlling the typesetting of text. */
    struct TypesettingParams {
        /** Vertical space between lines of text as factor of line height. */
        float line_spacing_factor = 1.5f;

        /** Maximum width of a line of text in pixels before line break. Optional, if left
         * undefined, there will be no automatic line breaks.
         */
        Opt<int32_t> max_width_pixels;
    };

    PreparedText prepare_text(FontId font,
                              std::string_view text_utf8,
                              const TypesettingParams& typesetting_params);
};

} // namespace Mg::gfx
