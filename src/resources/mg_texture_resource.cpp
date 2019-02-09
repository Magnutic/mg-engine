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

#include "mg/resources/mg_texture_resource.h"

#include <cstring> // memcpy
#include <stdexcept>
#include <vector>

#include "mg/core/mg_file_loader.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_resource_cache.h"

namespace Mg {

//--------------------------------------------------------------------------------------------------
// DDS format helpers
//--------------------------------------------------------------------------------------------------

// DDS_header.dwFlags
constexpr uint32_t DDSD_CAPS        = 0x00000001;
constexpr uint32_t DDSD_HEIGHT      = 0x00000002;
constexpr uint32_t DDSD_WIDTH       = 0x00000004;
constexpr uint32_t DDSD_PITCH       = 0x00000008;
constexpr uint32_t DDSD_PIXELFORMAT = 0x00001000;
constexpr uint32_t DDSD_MIPMAPCOUNT = 0x00020000;
constexpr uint32_t DDSD_LINEARSIZE  = 0x00080000;
constexpr uint32_t DDSD_DEPTH       = 0x00800000;

// DDS_header.ddspf.dwFlags
constexpr uint32_t DDPF_ALPHAPIXELS = 0x00000001;
constexpr uint32_t DDPF_FOURCC      = 0x00000004;
constexpr uint32_t DDPF_INDEXED     = 0x00000020;
constexpr uint32_t DDPF_RGB         = 0x00000040;

// DDS_header.dwCaps1
constexpr uint32_t DDSCAPS_COMPLEX = 0x00000008;
constexpr uint32_t DDSCAPS_TEXTURE = 0x00001000;
constexpr uint32_t DDSCAPS_MIPMAP  = 0x00400000;

// DDS_header.dwCaps2
constexpr uint32_t DDSCAPS2_CUBEMAP           = 0x00000200;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000;
constexpr uint32_t DDSCAPS2_VOLUME            = 0x00200000;

// DDS_HEADER.DDS_PIXELFORMAT.dwFourCC
constexpr uint32_t DDS  = 0x20534444;
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
    uint32_t        dwSize;
    uint32_t        dwFlags;
    uint32_t        dwHeight;
    uint32_t        dwWidth;
    uint32_t        dwPitchOrLinearSize;
    uint32_t        dwDepth;
    uint32_t        dwMipMapCount;
    uint32_t        dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t        dwCaps;
    uint32_t        dwCaps2;
    uint32_t        dwCaps3;
    uint32_t        dwCaps4;
    uint32_t        dwReserved2;
};

/** Determine pixel format of DDS file. Throws if format is unsupported. */
static TextureResource::PixelFormat dds_pf_to_pixel_format(const DDS_PIXELFORMAT& pf)
{
    if ((pf.dwFlags & DDPF_FOURCC) != 0) {
        switch (pf.dwFourCC) {
        case DXT1: return TextureResource::PixelFormat::DXT1;
        case DXT3: return TextureResource::PixelFormat::DXT3;
        case DXT5: return TextureResource::PixelFormat::DXT5;
        case ATI2: return TextureResource::PixelFormat::ATI2;
        default: break;
        }
    }

    bool rgb = ((pf.dwFlags & DDPF_RGB) != 0) && (pf.dwRBitMask == 0x00ff0000u) &&
               (pf.dwGBitMask == 0x0000ff00u) && (pf.dwBBitMask == 0x000000ffu);

    bool alpha = ((pf.dwFlags & DDPF_ALPHAPIXELS) != 0) && (pf.dwABitMask == 0xff000000u);

    if (rgb && alpha && pf.dwRGBBitCount == 32) { return TextureResource::PixelFormat::BGRA; }

    if (rgb && !alpha && pf.dwRGBBitCount == 24) { return TextureResource::PixelFormat::BGR; }

    throw std::runtime_error{
        "Unsupported DDS format. Supported formats: "
        "RGB8; ARGB8; DXT1; DXT3; DXT5; 3Dc (ATI2)."
    };
}

inline size_t block_size_by_format(TextureResource::PixelFormat pixel_format)
{
    switch (pixel_format) {
    case TextureResource::PixelFormat::DXT1: return 8;
    case TextureResource::PixelFormat::DXT3: return 16;
    case TextureResource::PixelFormat::DXT5: return 16;
    case TextureResource::PixelFormat::ATI2: return 16;
    case TextureResource::PixelFormat::BGR: return 48;
    case TextureResource::PixelFormat::BGRA: return 64;
    }

    throw std::runtime_error{ "Unexpected value for TextureResource::PixelFormat" };
}

/** Get the number of blocks in a mipmap of given dimensions. */
inline size_t num_blocks_by_img_size(TextureResource::DimT width, TextureResource::DimT height)
{
    return ((width + 3) / 4) * ((height + 3) / 4);
}

//--------------------------------------------------------------------------------------------------
// TextureResource implementation
//--------------------------------------------------------------------------------------------------

void TextureResource::load_resource(const ResourceDataLoader& loader)
{
    // Load raw data into temporary buffer
    std::vector<std::byte> data(loader.file_size());
    loader.load_file(data);

    // Init texture using raw data
    init(data, loader.allocator());
}

TextureResource::MipLevelData TextureResource::pixel_data(MipIndexT mip_index) const
{
    auto width  = m_format.width;
    auto height = m_format.height;

    size_t     offset     = 0;
    const auto block_size = block_size_by_format(m_format.pixel_format);
    auto       size       = num_blocks_by_img_size(width, height) * block_size;

    for (MipIndexT i = 0; i < mip_index; ++i) {
        offset += size;
        width /= 2;
        height /= 2;
        size = num_blocks_by_img_size(width, height) * block_size;
    }

    return MipLevelData{ span{ m_pixel_data }.subspan(offset, size), width, height };
}

// DDS loading
void TextureResource::init(span<const std::byte> dds_data, memory::DefragmentingAllocator& alloc)
{
    if (dds_data.length() < sizeof(DDS_HEADER)) {
        throw std::runtime_error{ "DDS file corrupt, missing data." };
    }

    // Read 4CC marker
    uint32_t four_cc;
    std::memcpy(&four_cc, dds_data.data(), 4);

    if (four_cc != DDS) { throw std::runtime_error{ "Not a DDS texture." }; }

    // Read header
    DDS_HEADER header{};
    std::memcpy(&header, dds_data.subspan(4).data(), sizeof(header));

    // Determine texture's pixel format
    PixelFormat pixel_format = dds_pf_to_pixel_format(header.ddspf);

    // Get location and size of pixel data
    const auto pixel_data_offset = sizeof(DDS_HEADER) + 4;
    MG_ASSERT_DEBUG(dds_data.length() > pixel_data_offset);
    const auto size = dds_data.length() - pixel_data_offset;

    // Size sanity check
    const auto width        = narrow<DimT>(header.dwWidth);
    const auto height       = narrow<DimT>(header.dwHeight);
    const auto num_mip_maps = narrow<MipIndexT>(header.dwMipMapCount);

    auto num_blocks = num_blocks_by_img_size(width, height);

    // Count size for all mipmaps
    {
        auto mip_width = width, mip_height = height;

        for (MipIndexT i = 1u; i < num_mip_maps; ++i) {
            mip_width /= 2;
            mip_height /= 2;
            num_blocks += num_blocks_by_img_size(mip_width, mip_height);
        }
    }

    const auto sane_size = num_blocks * block_size_by_format(pixel_format);

    if (size < sane_size) { throw std::runtime_error{ "DDS file corrupt, missing data." }; }

    if (size > sane_size) {
        const auto fname = resource_id().c_str();
        g_log.write_warning(
            fmt::format("TextureResource '{}': file has different length than "
                        "specified by format and is possibly corrupt.",
                        fname));
    }

    // Set up texture format info struct
    m_format.pixel_format = pixel_format;
    m_format.width        = width;
    m_format.height       = height;
    m_format.mip_levels   = num_mip_maps;

    // DDS files without mipmapping (sometimes?) claim to have 0 mipmaps,
    // but for consistency we always count the base image as a mipmap.
    if (m_format.mip_levels == 0) { m_format.mip_levels = 1; }

    // Copy pixel data
    span<const std::byte> data(&dds_data[pixel_data_offset], sane_size);
    m_pixel_data = alloc.alloc_copy(data.begin(), data.end());
}

} // namespace Mg
