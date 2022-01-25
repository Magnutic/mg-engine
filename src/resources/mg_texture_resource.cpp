//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_texture_resource.h"

#include "mg/core/mg_file_loader.h"
#include "mg/core/mg_log.h"
#include "mg/resource_cache/mg_resource_loading_input.h"
#include "mg/utils/mg_math_utils.h"

#include <cstring> // memcpy

namespace Mg {

namespace {

//--------------------------------------------------------------------------------------------------
// DDS format helpers
//--------------------------------------------------------------------------------------------------

// DDS_header.ddspf.dwFlags
constexpr uint32_t DDPF_ALPHAPIXELS = 0x00000001;
constexpr uint32_t DDPF_FOURCC = 0x00000004;
constexpr uint32_t DDPF_RGB = 0x00000040;

#if 0 // Presently unused constants

// DDS_header.ddspf.dwFlags
constexpr uint32_t DDPF_INDEXED = 0x00000020;

// DDS_header.dwFlags
constexpr uint32_t DDSD_CAPS = 0x00000001;
constexpr uint32_t DDSD_HEIGHT = 0x00000002;
constexpr uint32_t DDSD_WIDTH = 0x00000004;
constexpr uint32_t DDSD_PITCH = 0x00000008;
constexpr uint32_t DDSD_PIXELFORMAT = 0x00001000;
constexpr uint32_t DDSD_MIPMAPCOUNT = 0x00020000;
constexpr uint32_t DDSD_LINEARSIZE = 0x00080000;
constexpr uint32_t DDSD_DEPTH = 0x00800000;

// DDS_header.dwCaps1
constexpr uint32_t DDSCAPS_COMPLEX = 0x00000008;
constexpr uint32_t DDSCAPS_TEXTURE = 0x00001000;
constexpr uint32_t DDSCAPS_MIPMAP = 0x00400000;

// DDS_header.dwCaps2
constexpr uint32_t DDSCAPS2_CUBEMAP = 0x00000200;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000;
constexpr uint32_t DDSCAPS2_VOLUME = 0x00200000;

#endif

struct DDS_PIXELFORMAT {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

static_assert(std::is_trivially_copyable_v<DDS_PIXELFORMAT>);

struct DDS_HEADER {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    std::array<uint32_t, 11> dwReserved1;
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
};

static_assert(std::is_trivially_copyable_v<DDS_HEADER>);

struct PixelFormatResult {
    bool valid;
    gfx::PixelFormat format;
};

constexpr uint32_t make_fourcc(const span<const char> four_chars)
{
    MG_ASSERT(four_chars.size() == 4 || (four_chars.size() == 5 && four_chars.back() == '\0'));
    uint32_t out = 0u;
    out |= (static_cast<uint32_t>(four_chars[0]));
    out |= (static_cast<uint32_t>(four_chars[1]) << 8u);
    out |= (static_cast<uint32_t>(four_chars[2]) << 16u);
    out |= (static_cast<uint32_t>(four_chars[3]) << 24u);
    return out;
}

std::string decompose_fourcc(const uint32_t fourcc)
{
    std::string out;
    out.resize(4);
    out[0] = static_cast<char>(fourcc & 0xff);
    out[1] = static_cast<char>((fourcc >> 8u) & 0xff);
    out[2] = static_cast<char>((fourcc >> 16u) & 0xff);
    out[3] = static_cast<char>((fourcc >> 24u) & 0xff);
    return out;
}

static_assert(make_fourcc("DDS ") == 0x20534444);
const int test_dummy = (MG_ASSERT(decompose_fourcc(0x20534444) == "DDS "), 0);

/** Determine pixel format of DDS file. Nullopt if format is unsupported. */
PixelFormatResult dds_pf_to_pixel_format(const DDS_PIXELFORMAT& pf)
{
    if ((pf.dwFlags & DDPF_FOURCC) != 0) {
        switch (pf.dwFourCC) {
        case make_fourcc("DXT1"):
            return { true, gfx::PixelFormat::DXT1 };
        case make_fourcc("DXT3"):
            return { true, gfx::PixelFormat::DXT3 };
        case make_fourcc("DXT5"):
            return { true, gfx::PixelFormat::DXT5 };
        case make_fourcc("ATI1"):
            return { true, gfx::PixelFormat::ATI1 };
        case make_fourcc("ATI2"):
            return { true, gfx::PixelFormat::ATI2 };
        }
    }

    const bool rgb = ((pf.dwFlags & DDPF_RGB) != 0) && (pf.dwRBitMask == 0x00ff0000u) &&
                     (pf.dwGBitMask == 0x0000ff00u) && (pf.dwBBitMask == 0x000000ffu);

    const bool alpha = ((pf.dwFlags & DDPF_ALPHAPIXELS) != 0) && (pf.dwABitMask == 0xff000000u);

    if (rgb && alpha && pf.dwRGBBitCount == 32) {
        return { true, gfx::PixelFormat::BGRA };
    }

    if (rgb && !alpha && pf.dwRGBBitCount == 24) {
        return { true, gfx::PixelFormat::BGR };
    }

    return { false, {} };
}

size_t block_size_by_format(gfx::PixelFormat pixel_format)
{
    switch (pixel_format) {
    case gfx::PixelFormat::DXT1:
        return 8;
    case gfx::PixelFormat::DXT3:
        return 16;
    case gfx::PixelFormat::DXT5:
        return 16;
    case gfx::PixelFormat::ATI1:
        return 8;
    case gfx::PixelFormat::ATI2:
        return 16;
    case gfx::PixelFormat::BGR:
        return 48;
    case gfx::PixelFormat::BGRA:
        return 64;
    }

    MG_ASSERT(false && "Unexpected value for gfx::PixelFormat");
}

/** Get the number of blocks in a mipmap of given dimensions. */
size_t num_blocks_by_img_size(TextureResource::DimT width, TextureResource::DimT height)
{
    return ((width + 3u) / 4) * ((height + 3u) / 4);
}

} // namespace

//--------------------------------------------------------------------------------------------------
// TextureResource implementation
//--------------------------------------------------------------------------------------------------

LoadResourceResult TextureResource::load_resource_impl(ResourceLoadingInput& input)
{
    const span<const std::byte> dds_data = input.resource_data();

    if (dds_data.length() < sizeof(DDS_HEADER)) {
        return LoadResourceResult::data_error("DDS file corrupt, missing data.");
    }

    // Read file-type identifier four-character code
    uint32_t four_cc{};
    std::memcpy(&four_cc, dds_data.data(), sizeof(four_cc));

    if (four_cc != make_fourcc("DDS ")) {
        return LoadResourceResult::data_error("Not a DDS texture.");
    }

    // Read header
    DDS_HEADER header{};
    std::memcpy(&header, dds_data.subspan(sizeof(four_cc)).data(), sizeof(header));

    // Determine texture's pixel format
    auto [valid, pixel_format] = dds_pf_to_pixel_format(header.ddspf);
    if (!valid) {
        if (header.ddspf.dwFourCC == make_fourcc("DX10")) {
            return LoadResourceResult::data_error("DX10 DDS files are currently unsupported.");
        }
        return LoadResourceResult::data_error(
            fmt::format("Unsupported DDS format: {}", decompose_fourcc(header.ddspf.dwFourCC)));
    }

    // Get location and size of pixel data
    const auto pixel_data_offset = sizeof(DDS_HEADER) + sizeof(four_cc);
    MG_ASSERT_DEBUG(dds_data.length() > pixel_data_offset);
    const auto size = dds_data.length() - pixel_data_offset;

    // Size sanity check
    const auto width = narrow<DimT>(header.dwWidth);
    const auto height = narrow<DimT>(header.dwHeight);
    const auto num_mip_maps = narrow<MipIndexT>(header.dwMipMapCount);

    auto num_blocks = num_blocks_by_img_size(width, height);

    // Count size for all mipmaps
    {
        auto mip_width = width;
        auto mip_height = height;

        for (MipIndexT i = 1u; i < num_mip_maps; ++i) {
            mip_width = max(1u, mip_width / 2);
            mip_height = max(1u, mip_height / 2);
            num_blocks += num_blocks_by_img_size(mip_width, mip_height);
        }
    }

    // The expected size of the image data. Actually an upper bound, since non-block-compressed
    // textures may not be evenly divisible by the block size; as a result, num_blocks_by_img_size
    // may have rounded up the number of blocks.
    const auto sane_size = num_blocks * block_size_by_format(pixel_format);

    if (size < sane_size) {
        return LoadResourceResult::data_error("DDS file corrupt, missing data.");
    }

    if (size > sane_size) {
        const auto fname = resource_id().c_str();
        log.warning(
            "TextureResource '{}': file has different length than "
            "specified by format and is possibly corrupt.",
            fname);
    }

    // Set up texture format info struct
    m_format.pixel_format = pixel_format;
    m_format.width = width;
    m_format.height = height;
    m_format.mip_levels = num_mip_maps;

    // DDS files without mipmapping (sometimes?) claim to have 0 mipmaps,
    // but for consistency we always count the base image as a mipmap.
    if (m_format.mip_levels == 0) {
        m_format.mip_levels = 1;
    }

    // Copy pixel data
    span<const std::byte> data(&dds_data[pixel_data_offset], sane_size);
    m_pixel_data = Array<std::byte>::make_copy(data);

    return LoadResourceResult::success();
}

TextureResource::MipLevelData TextureResource::pixel_data(MipIndexT mip_index) const noexcept
{
    auto width = m_format.width;
    auto height = m_format.height;

    size_t offset = 0;
    const auto block_size = block_size_by_format(m_format.pixel_format);
    auto size = num_blocks_by_img_size(width, height) * block_size;

    for (MipIndexT i = 0; i < mip_index; ++i) {
        offset += size;
        width = max(1u, width / 2);
        height = max(1u, height / 2);
        size = num_blocks_by_img_size(width, height) * block_size;
    }

    return MipLevelData{ span(m_pixel_data).subspan(offset, size), width, height };
}

} // namespace Mg
