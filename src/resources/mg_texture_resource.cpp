//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_texture_resource.h"

#include "mg/core/mg_file_loader.h"
#include "mg/core/mg_log.h"
#include "mg/resource_cache/mg_resource_loading_input.h"
#include "mg/utils/mg_math_utils.h"

#include <fmt/core.h>

#include <cstring> // memcpy

namespace Mg {

namespace {

//--------------------------------------------------------------------------------------------------
// DDS format helpers
//--------------------------------------------------------------------------------------------------

// DDS_header.dwFlags
constexpr uint32_t DDSD_CAPS = 0x00000001;
constexpr uint32_t DDSD_HEIGHT = 0x00000002;
constexpr uint32_t DDSD_WIDTH = 0x00000004;
constexpr uint32_t DDSD_PITCH = 0x00000008;
constexpr uint32_t DDSD_PIXELFORMAT = 0x00001000;
constexpr uint32_t DDSD_MIPMAPCOUNT = 0x00020000;
constexpr uint32_t DDSD_LINEARSIZE = 0x00080000;
constexpr uint32_t DDSD_DEPTH = 0x00800000;

// DDS_header.ddspf.dwFlags
constexpr uint32_t DDPF_ALPHAPIXELS = 0x00000001;
constexpr uint32_t DDPF_FOURCC = 0x00000004;
constexpr uint32_t DDPF_INDEXED = 0x00000020;
constexpr uint32_t DDPF_RGB = 0x00000040;

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

// DDS_HEADER.DDS_PIXELFORMAT.dwFourCC
constexpr uint32_t DDS = 0x20534444;
constexpr uint32_t DXT1 = 0x31545844;
constexpr uint32_t DXT2 = 0x32545844;
constexpr uint32_t DXT3 = 0x33545844;
constexpr uint32_t DXT4 = 0x34545844;
constexpr uint32_t DXT5 = 0x35545844;
constexpr uint32_t ATI2 = 0x32495441;

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

struct DDS_HEADER {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
};

struct PixelFormatResult {
    bool valid;
    TextureResource::PixelFormat format;
};
/** Determine pixel format of DDS file. Nullopt if format is unsupported. */
PixelFormatResult dds_pf_to_pixel_format(const DDS_PIXELFORMAT& pf)
{
    if ((pf.dwFlags & DDPF_FOURCC) != 0) {
        switch (pf.dwFourCC) {
        case DXT1:
            return { true, TextureResource::PixelFormat::DXT1 };
        case DXT3:
            return { true, TextureResource::PixelFormat::DXT3 };
        case DXT5:
            return { true, TextureResource::PixelFormat::DXT5 };
        case ATI2:
            return { true, TextureResource::PixelFormat::ATI2 };
        default:
            break;
        }
    }

    const bool rgb = ((pf.dwFlags & DDPF_RGB) != 0) && (pf.dwRBitMask == 0x00ff0000u) &&
                     (pf.dwGBitMask == 0x0000ff00u) && (pf.dwBBitMask == 0x000000ffu);

    const bool alpha = ((pf.dwFlags & DDPF_ALPHAPIXELS) != 0) && (pf.dwABitMask == 0xff000000u);

    if (rgb && alpha && pf.dwRGBBitCount == 32) {
        return { true, TextureResource::PixelFormat::BGRA };
    }

    if (rgb && !alpha && pf.dwRGBBitCount == 24) {
        return { true, TextureResource::PixelFormat::BGR };
    }

    return { false, {} };
}

size_t block_size_by_format(TextureResource::PixelFormat pixel_format)
{
    switch (pixel_format) {
    case TextureResource::PixelFormat::DXT1:
        return 8;
    case TextureResource::PixelFormat::DXT3:
        return 16;
    case TextureResource::PixelFormat::DXT5:
        return 16;
    case TextureResource::PixelFormat::ATI2:
        return 16;
    case TextureResource::PixelFormat::BGR:
        return 48;
    case TextureResource::PixelFormat::BGRA:
        return 64;
    }

    MG_ASSERT(false && "Unexpected value for TextureResource::PixelFormat");
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

    // Read 4CC marker
    uint32_t four_cc;
    std::memcpy(&four_cc, dds_data.data(), 4);

    if (four_cc != DDS) {
        return LoadResourceResult::data_error("Not a DDS texture.");
    }

    // Read header
    DDS_HEADER header{};
    std::memcpy(&header, dds_data.subspan(4).data(), sizeof(header));

    // Determine texture's pixel format
    auto [valid, pixel_format] = dds_pf_to_pixel_format(header.ddspf);
    if (!valid) {
        return LoadResourceResult::data_error("Unsupported DDS format.");
    }

    // Get location and size of pixel data
    const auto pixel_data_offset = sizeof(DDS_HEADER) + 4;
    MG_ASSERT_DEBUG(dds_data.length() > pixel_data_offset);
    const auto size = dds_data.length() - pixel_data_offset;

    // Size sanity check
    const auto width = narrow<DimT>(header.dwWidth);
    const auto height = narrow<DimT>(header.dwHeight);
    const auto num_mip_maps = narrow<MipIndexT>(header.dwMipMapCount);

    auto num_blocks = num_blocks_by_img_size(width, height);

    // Count size for all mipmaps
    {
        auto mip_width = width, mip_height = height;

        for (MipIndexT i = 1u; i < num_mip_maps; ++i) {
            mip_width = max(1u, mip_width / 2);
            mip_height = max(1u, mip_height / 2);
            num_blocks += num_blocks_by_img_size(mip_width, mip_height);
        }
    }

    const auto sane_size = num_blocks * block_size_by_format(pixel_format);

    if (size < sane_size) {
        return LoadResourceResult::data_error("DDS file corrupt, missing data.");
    }

    if (size > sane_size) {
        const auto fname = resource_id().c_str();
        g_log.write_warning(
            fmt::format("TextureResource '{}': file has different length than "
                        "specified by format and is possibly corrupt.",
                        fname));
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

    return MipLevelData{ span{ m_pixel_data }.subspan(offset, size), width, height };
}

} // namespace Mg
