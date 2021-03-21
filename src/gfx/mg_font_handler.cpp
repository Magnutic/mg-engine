//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_font_handler.h"

#include "mg/containers/mg_array.h"
#include "mg/containers/mg_flat_map.h"
#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resources/mg_font_resource.h"
#include "mg/utils/mg_file_io.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"

#include "opengl/mg_glad.h" // TODO temp

//--------------------------------------------------------------------------------------------------
// Include stb_truetype.
// We do this in a slightly evil manner:
// To avoid risk of symbol conflict in the not-too-unlikely case of linking with another object that
// also uses stb_truetype, we wrap the #include in a custom namespace.
// For this to work, we must ensure the stb headers themselves do not include any standard library
// headers.
// As long as we provide implementation macros for all functions used by the stb headers, this will
// work.

#include <assert.h> // NOLINT(modernize-deprecated-headers)
#include <math.h>   // NOLINT(modernize-deprecated-headers)
#include <stdlib.h> // NOLINT(modernize-deprecated-headers)
#include <string.h> // NOLINT(modernize-deprecated-headers)

// Definitions for stb_rect_pack
#define STBRP_SORT qsort    // NOLINT
#define STBRP_ASSERT assert // NOLINT

// Definitions for stb_truetype
#define STBTT_ifloor(x) ((int)::floor(x))           // NOLINT
#define STBTT_iceil(x) ((int)::ceil(x))             // NOLINT
#define STBTT_sqrt(x) ::sqrt(x)                     // NOLINT
#define STBTT_pow(x, y) ::pow(x, y)                 // NOLINT
#define STBTT_fmod(x, y) ::fmod(x, y)               // NOLINT
#define STBTT_cos(x) ::cos(x)                       // NOLINT
#define STBTT_acos(x) ::acos(x)                     // NOLINT
#define STBTT_fabs(x) ::fabs(x)                     // NOLINT
#define STBTT_malloc(x, u) ((void)(u), ::malloc(x)) // NOLINT
#define STBTT_free(x, u) ((void)(u), ::free(x))     // NOLINT
#define STBTT_assert(x) assert(x)                   // NOLINT
#define STBTT_strlen(x) ::strlen(x)                 // NOLINT
#define STBTT_memcpy ::memcpy                       // NOLINT
#define STBTT_memset ::memset                       // NOLINT

#define STB_TRUETYPE_IMPLEMENTATION 1  // NOLINT
#define STB_RECT_PACK_IMPLEMENTATION 1 // NOLINT

// Namespace stb-library definitions to avoid potential symbol conflicts.
namespace Mg::stb {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

// Note: stb_rect_pack must be included before truetype, as the latter will use a fallback if the
// former is not included.
#include <stb_rect_pack.h>
#include <stb_truetype.h>

#pragma GCC diagnostic pop

} // namespace Mg::stb

using namespace Mg::stb;

//--------------------------------------------------------------------------------------------------

#include <fmt/core.h>

#include <glm/vec2.hpp>

#include <atomic>
#include <numeric>
#include <string>
#include <vector>

namespace Mg::gfx {

struct BitmapFontData {
    // Texture with packed glyph rasters.
    TextureHandle::Owner texture;
    int texture_width = 0;
    int texture_height = 0;

    // Font size (letter height) in pixels.
    int font_size_pixels = 0;

    // Indexable array of data describing where in texture a glyph resides
    Array<stbtt_packedchar> packed_chars;

    // The range of unicode code points represented in texture and packed_chars.
    small_vector<UnicodeRange, 5> unicode_ranges;
};

//--------------------------------------------------------------------------------------------------
// Private definitions
//--------------------------------------------------------------------------------------------------

namespace {

// Initial size of rasterised font textures. They grow as needed to fit requested glyphs.
constexpr int k_initial_font_texture_width = 128;
constexpr int k_initial_font_texture_height = 128;

// Character to substitute when trying to display an unsupported codepoint.
constexpr char32_t k_substitution_character = U'?';

Opt<int32_t> get_packedchar_index(const BitmapFontData& font, char32_t codepoint)
{
    int32_t result = 0;
    for (const UnicodeRange& range : font.unicode_ranges) {
        if (contains_codepoint(range, codepoint)) {
            result += narrow<int>(codepoint - range.start);
            return result;
        }
        result += narrow<int32_t>(range.length);
    }

    return nullopt;
}

class FontPacker {
public:
    void pack(ResourceHandle<FontResource> font_resource,
              span<const UnicodeRange> unicode_ranges,
              const int font_size_pixels,
              BitmapFontData& font)
    {
        // Set initial texture size.
        m_texture_width = k_initial_font_texture_width;
        m_texture_height = k_initial_font_texture_height;

        // Load font data.
        ResourceAccessGuard resource_access{ font_resource };

        auto merged_ranges = merge_overlapping_ranges(unicode_ranges);

        { // Ensure the substitution character is present in merged_ranges.
            auto contains_subsistution_char = [](UnicodeRange r) {
                return contains_codepoint(r, k_substitution_character);
            };

            const bool has_substitution_char = count_if(merged_ranges,
                                                        contains_subsistution_char) == 1;
            if (!has_substitution_char) {
                merged_ranges.push_back({ k_substitution_character, 1 });
            }
        }

        // Pack into a texture.
        pack_impl(resource_access->data(), merged_ranges, font_size_pixels, font);

        log.verbose("Packed font {}:{} into {}x{} texture.",
                    font_resource.resource_id().str_view(),
                    font_size_pixels,
                    m_texture_width,
                    m_texture_height);
    }

private:
    void pack_impl(const span<const std::byte>& font_data,
                   const span<const UnicodeRange>& unicode_ranges,
                   const int font_size_pixels,
                   BitmapFontData& font)
    {
        {
            auto texture_data =
                Array<uint8_t>::make(narrow<size_t>(m_texture_width * m_texture_height));
            stbtt_pack_context pack_context;

            std::copy(unicode_ranges.begin(),
                      unicode_ranges.end(),
                      std::back_inserter(font.unicode_ranges));

            const size_t num_packed_chars =
                std::accumulate(unicode_ranges.begin(),
                                unicode_ranges.end(),
                                size_t(0),
                                [](size_t acc, UnicodeRange range) { return acc + range.length; });

            font.packed_chars = Array<stbtt_packedchar>::make(num_packed_chars);

            const int stride_in_bytes = 0;
            const int padding = 1;
            void* const alloc_context = nullptr;
            if (0 == stbtt_PackBegin(&pack_context,
                                     texture_data.data(),
                                     m_texture_width,
                                     m_texture_height,
                                     stride_in_bytes,
                                     padding,
                                     alloc_context)) {
                log.error("Failed to initiate STBTT font packing context.");
                throw RuntimeError{};
            }

            const auto font_index = 0;
            const auto font_size = static_cast<float>(font_size_pixels);

            bool packing_succeeded = true;
            size_t packed_char_offset = 0;

            for (UnicodeRange unicode_range : unicode_ranges) {
                const auto first_codepoint = narrow<int>(unicode_range.start);
                const auto num_codepoints = narrow<int>(unicode_range.length);
                const auto* font_data_ptr = reinterpret_cast<const uint8_t*>(font_data.data());

                auto* chardata_for_range = &font.packed_chars[packed_char_offset];
                const auto pack_result = stbtt_PackFontRange(&pack_context,
                                                             font_data_ptr,
                                                             font_index,
                                                             font_size,
                                                             first_codepoint,
                                                             num_codepoints,
                                                             chardata_for_range);
                if (pack_result == 0) {
                    packing_succeeded = false;
                    break;
                }

                packed_char_offset += unicode_range.length;
            }

            stbtt_PackEnd(&pack_context);

            if (packing_succeeded) {
                font.texture = make_texture(pack_context);
                font.texture_width = m_texture_width;
                font.texture_height = m_texture_height;
                return;
            }
        }

        // If failed, try again recursively with larger texture.
        grow_texture();
        pack_impl(font_data, unicode_ranges, font_size_pixels, font);
    }

    // Create texture for the given font data.
    TextureHandle::Owner make_texture(stbtt_pack_context& context) const
    {
        // Upload rasterized font texture to GPU.
        GLuint gl_texture_id = 0;
        glGenTextures(1, &gl_texture_id);
        glBindTexture(GL_TEXTURE_2D, gl_texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_R8,
                     m_texture_width,
                     m_texture_height,
                     0,
                     GL_RED,
                     GL_UNSIGNED_BYTE,
                     context.pixels);
        return TextureHandle::Owner{ gl_texture_id };
    }

    void grow_texture()
    {
        int& side = (m_texture_width <= m_texture_height) ? m_texture_width : m_texture_height;
        side *= 2;
    }

    int m_texture_width = 0;
    int m_texture_height = 0;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// PreparedText
//--------------------------------------------------------------------------------------------------

PreparedText::~PreparedText()
{
    const auto vbo_id = narrow<GLuint>(m_gpu_data.vertex_buffer.get());
    const auto vao_id = narrow<GLuint>(m_gpu_data.vertex_array.get());
    glDeleteBuffers(1, &vbo_id);
    glDeleteVertexArrays(1, &vao_id);
}

//--------------------------------------------------------------------------------------------------
// BitmapFont
//--------------------------------------------------------------------------------------------------

BitmapFont::BitmapFont(ResourceHandle<FontResource> font,
                       const int font_size_pixels,
                       span<const UnicodeRange> unicode_ranges)
{
    impl().font_size_pixels = font_size_pixels;
    FontPacker packer;
    packer.pack(font, unicode_ranges, font_size_pixels, impl());
}

BitmapFont::~BitmapFont() = default;

// Implementation of BitmapFont::prepare_text

namespace {

struct BreakLinesResult {
    Array<stbtt_aligned_quad> char_quads;
    float width;
    float height;
    size_t num_lines;
};

// Break text into multiple lines at '\n'-codepoints and wherever the line would exceed
// `max_width_pixels`.
BreakLinesResult break_lines(const std::u32string_view& text_codepoints,
                             span<const stbtt_aligned_quad> single_line_quads,
                             const Opt<int32_t> max_width_pixels,
                             const float line_height,
                             const float line_spacing_factor)
{
    MG_ASSERT(text_codepoints.size() == single_line_quads.size());

    auto result_quads = Array<stbtt_aligned_quad>::make_copy(single_line_quads);
    float width = 0.0f;
    size_t num_lines = 1;

    // Index of the quad where the current lines starts.
    size_t line_start_index = 0;

    // Index of the most recent whitespace glyph quad. We always break at whitespace boundaries here
    // (no hyphenation of forced line breaks).
    size_t last_whitespace_index = 0;

    // The single_line_quads include the glyph quads' vertex positions arranged in a single line. We
    // will split into multiple lines by modifying these positions. Hence, we track the accumulated
    // offset caused by line breaks in the x and y axes and apply to each position. Every time we
    // break a line, we add the line height to the y_offset and subtract the width of the line to
    // make sure the next line starts where it should.
    float x_offset = 0.0f;
    float y_offset = 0.0f;

    // Whether this glyph should be the start of a new line.
    bool start_new_line = false;

    size_t i = 0;
    while (i < single_line_quads.size()) {
        if (is_whitespace(text_codepoints[i])) {
            last_whitespace_index = i;

            // Skip leading whitespace after line break unless it is another line break.
            if (start_new_line && text_codepoints[i] != '\n') {
                ++i;
                continue;
            }
        }

        const stbtt_aligned_quad& q = single_line_quads[i];

        if (start_new_line) {
            start_new_line = false;
            line_start_index = i;
            // Accumulate offset to move the following quads to down and inward.
            // TODO: consider whether this works for right-to-left languages.
            x_offset = -q.x0;
            y_offset += line_height * line_spacing_factor;
            ++num_lines;
        }

        if (text_codepoints[i] == '\n') {
            start_new_line = true;
            ++i;
            continue;
        }

        const bool is_first_char_of_line = i == line_start_index;
        const bool is_past_width_limit = max_width_pixels.has_value() &&
                                         (q.x1 + x_offset) >
                                             static_cast<float>(max_width_pixels.value());

        if (!is_first_char_of_line && is_past_width_limit) {
            if (last_whitespace_index > line_start_index) {
                i = last_whitespace_index + 1;
            }
            start_new_line = true;
            continue;
        }

        // Apply accumulated offsets.
        result_quads[i].x0 = q.x0 + x_offset;
        result_quads[i].x1 = q.x1 + x_offset;
        result_quads[i].y0 = q.y0 + y_offset;
        result_quads[i].y1 = q.y1 + y_offset;

        // Track the maximum horizontal extents of the text after adding line breaks.
        width = max(width, q.x1);
        ++i;
    }

    const float height = line_height + y_offset;
    return { std::move(result_quads), width, height, num_lines };
};

// Convert to UTF32 and filter out unprintable characters.
std::u32string convert_and_filter(std::string_view text)
{
    // Convert to codepoints.
    bool utf8_error = false;
    std::u32string codepoints = utf8_to_utf32(text, &utf8_error);
    if (utf8_error) {
        auto msg = fmt::format("FontHandler::prepare_text: invalid UTF-8 in string '{}'.", text);
        log.warning(msg);
    }

    // Filter out ASCII control characters, except for tabs, which we will turn into four spaces,
    // and line feeds, which are later handled by break_lines.
    auto is_ascii_ctrl_code = [](const char32_t c) { return (c != 10 && c < 32) || c == 127; };
    const bool should_be_filtered =
        std::find_if(codepoints.begin(), codepoints.end(), is_ascii_ctrl_code) != codepoints.end();

    if (should_be_filtered) {
        std::u32string filtered_text;
        for (const char32_t c : codepoints) {
            if (!is_ascii_ctrl_code(c)) {
                filtered_text += c;
            }
            else if (c == '\t') {
                filtered_text += U"    ";
            }
        }
        codepoints = filtered_text;
    }

    return codepoints;
}

} // namespace

PreparedText BitmapFont::prepare_text(std::string_view text_utf8,
                                      const TypeSetting& typesetting) const
{
    // Convert text to sequence of code points (while filtering out unprintable characters).
    const std::u32string text_codepoints = convert_and_filter(text_utf8);
    const auto line_height = static_cast<float>(font_size_pixels());

    // Get texture for font.
    MG_ASSERT(impl().texture.handle != TextureHandle::null_handle());

    // Get quads for each codepoint in the string.
    auto char_quads = Array<stbtt_aligned_quad>::make(text_codepoints.size());
    {
        float x = 0.0f;
        float y = line_height;

        for (size_t i = 0; i < text_codepoints.size(); ++i) {
            auto codepoint = text_codepoints[i];
            stbtt_aligned_quad& quad = char_quads[i];

            // Do not try to print newline characters, but put a space instead to ensure one-to-one
            // relationship between char_quads and text_codepoints.
            codepoint = (codepoint == '\n' ? ' ' : codepoint);

            // Get index of packedchar corresponding to codepoint, or that of '?' if not present.
            const auto packedchar_index = get_packedchar_index(impl(), codepoint)
                                              .value_or(get_packedchar_index(impl(), '?').value());
            constexpr int align_to_integer = 1;
            stbtt_GetPackedQuad(impl().packed_chars.data(),
                                impl().texture_width,
                                impl().texture_height,
                                packedchar_index,
                                &x,
                                &y,
                                &quad,
                                align_to_integer);
        }
    }

    // Break quads into multiple lines.
    BreakLinesResult break_lines_result = break_lines(text_codepoints,
                                                      char_quads,
                                                      typesetting.max_width_pixels,
                                                      line_height,
                                                      typesetting.line_spacing_factor);
    char_quads = std::move(break_lines_result.char_quads);
    const float width = break_lines_result.width;
    const float height = break_lines_result.height;

    // Prepare vertex data.
    struct Vertex {
        glm::vec2 position;
        glm::vec2 tex_coord;
    };

    static_assert(sizeof(Vertex) == 2u * sizeof(glm::vec2)); // Assert no packing.

    static constexpr size_t verts_per_char = 6u;
    auto vertices = Array<Vertex>::make(text_codepoints.size() * verts_per_char);

    for (size_t i = 0; i < char_quads.size(); ++i) {
        const stbtt_aligned_quad& q = char_quads[i];
        const size_t offset = i * verts_per_char;

        // Normalise vertex positions into [0.0, 1.0] to simplify transformations (width and height
        // is stored along with the text so that it can be scaled appropriately in the vertex
        // shader).
        // Flipped Y-axis compared with what stb_truetype expects.
        const float x0 = q.x0 / width;
        const float x1 = q.x1 / width;
        const float y0 = 1.0f - q.y0 / height;
        const float y1 = 1.0f - q.y1 / height;
        vertices[offset + 0] = { { x0, y1 }, { q.s0, q.t1 } };
        vertices[offset + 1] = { { x1, y1 }, { q.s1, q.t1 } };
        vertices[offset + 2] = { { x1, y0 }, { q.s1, q.t0 } };
        vertices[offset + 3] = { { x0, y1 }, { q.s0, q.t1 } };
        vertices[offset + 4] = { { x1, y0 }, { q.s1, q.t0 } };
        vertices[offset + 5] = { { x0, y0 }, { q.s0, q.t0 } };
    }

    // Create OpenGL buffers.
    // TODO move into GL-specific private header.
    GLuint gl_vertex_array_id = 0;
    glGenVertexArrays(1, &gl_vertex_array_id);
    glBindVertexArray(gl_vertex_array_id);

    GLuint gl_buffer_id = 0;
    glGenBuffers(1, &gl_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_id);
    glBufferData(GL_ARRAY_BUFFER,
                 narrow<GLsizei>(sizeof(Vertex) * vertices.size()),
                 static_cast<const GLvoid*>(vertices.data()),
                 GL_STATIC_DRAW);

    const auto stride = narrow<GLsizei>(sizeof(Vertex));
    const GLvoid* position_offset = nullptr;
    const auto* texcoord_offset = reinterpret_cast<const GLvoid*>(sizeof(glm::vec2));

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, position_offset);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, texcoord_offset);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    PreparedText::GpuData gpu_data;
    gpu_data.texture = impl().texture.handle;
    gpu_data.vertex_buffer = BufferHandle(gl_buffer_id);
    gpu_data.vertex_array = VertexArrayHandle(gl_vertex_array_id);
    return PreparedText(gpu_data, width, height, text_codepoints.size());
}

span<const UnicodeRange> BitmapFont::contained_ranges() const
{
    return impl().unicode_ranges;
}

int BitmapFont::font_size_pixels() const
{
    return impl().font_size_pixels;
}

} // namespace Mg::gfx
