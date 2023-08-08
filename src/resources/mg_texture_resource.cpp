//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_texture_resource.h"

#include "mg/core/mg_file_loader.h"
#include "mg/core/mg_log.h"
#include "mg/gfx/mg_texture_related_types.h"
#include "mg/resource_cache/mg_resource_loading_input.h"
#include "mg/utils/mg_math_utils.h"

#include <cstring> // memcpy
#include <type_traits>

namespace Mg {

namespace {

//--------------------------------------------------------------------------------------------------
// DDS format helpers
//--------------------------------------------------------------------------------------------------

// DDS_header.ddspf.dwFlags
constexpr uint32_t DDPF_ALPHAPIXELS = 0x00000001;
constexpr uint32_t DDPF_FOURCC = 0x00000004;
constexpr uint32_t DDPF_RGB = 0x00000040;

#if 0 // currently unused
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
#endif

// DDS_header.dwCaps2
constexpr uint32_t DDSCAPS2_CUBEMAP = 0x00000200;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000;
constexpr uint32_t DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000;
constexpr uint32_t DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000;
#if 0 // Currently unused
constexpr uint32_t DDSCAPS2_VOLUME = 0x00200000;
#endif

// Stored image format (for DX10 DDS files).
enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
    DXGI_FORMAT_R32G32B32A32_UINT = 3,
    DXGI_FORMAT_R32G32B32A32_SINT = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS = 5,
    DXGI_FORMAT_R32G32B32_FLOAT = 6,
    DXGI_FORMAT_R32G32B32_UINT = 7,
    DXGI_FORMAT_R32G32B32_SINT = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM = 11,
    DXGI_FORMAT_R16G16B16A16_UINT = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM = 13,
    DXGI_FORMAT_R16G16B16A16_SINT = 14,
    DXGI_FORMAT_R32G32_TYPELESS = 15,
    DXGI_FORMAT_R32G32_FLOAT = 16,
    DXGI_FORMAT_R32G32_UINT = 17,
    DXGI_FORMAT_R32G32_SINT = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM = 24,
    DXGI_FORMAT_R10G10B10A2_UINT = 25,
    DXGI_FORMAT_R11G11B10_FLOAT = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
    DXGI_FORMAT_R8G8B8A8_UINT = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM = 31,
    DXGI_FORMAT_R8G8B8A8_SINT = 32,
    DXGI_FORMAT_R16G16_TYPELESS = 33,
    DXGI_FORMAT_R16G16_FLOAT = 34,
    DXGI_FORMAT_R16G16_UNORM = 35,
    DXGI_FORMAT_R16G16_UINT = 36,
    DXGI_FORMAT_R16G16_SNORM = 37,
    DXGI_FORMAT_R16G16_SINT = 38,
    DXGI_FORMAT_R32_TYPELESS = 39,
    DXGI_FORMAT_D32_FLOAT = 40,
    DXGI_FORMAT_R32_FLOAT = 41,
    DXGI_FORMAT_R32_UINT = 42,
    DXGI_FORMAT_R32_SINT = 43,
    DXGI_FORMAT_R24G8_TYPELESS = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
    DXGI_FORMAT_R8G8_TYPELESS = 48,
    DXGI_FORMAT_R8G8_UNORM = 49,
    DXGI_FORMAT_R8G8_UINT = 50,
    DXGI_FORMAT_R8G8_SNORM = 51,
    DXGI_FORMAT_R8G8_SINT = 52,
    DXGI_FORMAT_R16_TYPELESS = 53,
    DXGI_FORMAT_R16_FLOAT = 54,
    DXGI_FORMAT_D16_UNORM = 55,
    DXGI_FORMAT_R16_UNORM = 56,
    DXGI_FORMAT_R16_UINT = 57,
    DXGI_FORMAT_R16_SNORM = 58,
    DXGI_FORMAT_R16_SINT = 59,
    DXGI_FORMAT_R8_TYPELESS = 60,
    DXGI_FORMAT_R8_UNORM = 61,
    DXGI_FORMAT_R8_UINT = 62,
    DXGI_FORMAT_R8_SNORM = 63,
    DXGI_FORMAT_R8_SINT = 64,
    DXGI_FORMAT_A8_UNORM = 65,
    DXGI_FORMAT_R1_UNORM = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
    DXGI_FORMAT_BC1_TYPELESS = 70,
    DXGI_FORMAT_BC1_UNORM = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB = 72,
    DXGI_FORMAT_BC2_TYPELESS = 73,
    DXGI_FORMAT_BC2_UNORM = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB = 75,
    DXGI_FORMAT_BC3_TYPELESS = 76,
    DXGI_FORMAT_BC3_UNORM = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB = 78,
    DXGI_FORMAT_BC4_TYPELESS = 79,
    DXGI_FORMAT_BC4_UNORM = 80,
    DXGI_FORMAT_BC4_SNORM = 81,
    DXGI_FORMAT_BC5_TYPELESS = 82,
    DXGI_FORMAT_BC5_UNORM = 83,
    DXGI_FORMAT_BC5_SNORM = 84,
    DXGI_FORMAT_B5G6R5_UNORM = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
    DXGI_FORMAT_BC6H_TYPELESS = 94,
    DXGI_FORMAT_BC6H_UF16 = 95,
    DXGI_FORMAT_BC6H_SF16 = 96,
    DXGI_FORMAT_BC7_TYPELESS = 97,
    DXGI_FORMAT_BC7_UNORM = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB = 99,
    DXGI_FORMAT_AYUV = 100,
    DXGI_FORMAT_Y410 = 101,
    DXGI_FORMAT_Y416 = 102,
    DXGI_FORMAT_NV12 = 103,
    DXGI_FORMAT_P010 = 104,
    DXGI_FORMAT_P016 = 105,
    DXGI_FORMAT_420_OPAQUE = 106,
    DXGI_FORMAT_YUY2 = 107,
    DXGI_FORMAT_Y210 = 108,
    DXGI_FORMAT_Y216 = 109,
    DXGI_FORMAT_NV11 = 110,
    DXGI_FORMAT_AI44 = 111,
    DXGI_FORMAT_IA44 = 112,
    DXGI_FORMAT_P8 = 113,
    DXGI_FORMAT_A8P8 = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM = 115,
    DXGI_FORMAT_P208 = 130,
    DXGI_FORMAT_V208 = 131,
    DXGI_FORMAT_V408 = 132,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
    DXGI_FORMAT_FORCE_UINT = 0xffffffff
};

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

enum D3D10_RESOURCE_DIMENSION {
    D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
    D3D10_RESOURCE_DIMENSION_BUFFER = 1,
    D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
    D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
    D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
};

// DDS file header:
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

// Header extension for DX10 DDS files.
struct DDS_HEADER_DXT10 {
    DXGI_FORMAT dxgiFormat;
    D3D10_RESOURCE_DIMENSION resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};

static_assert(std::is_trivially_copyable_v<DDS_HEADER>);
static_assert(std::is_trivially_copyable_v<DDS_HEADER_DXT10>);

struct PixelFormatResult {
    bool valid;
    gfx::PixelFormat format;
};

constexpr uint32_t make_fourcc(const std::span<const char> four_chars)
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
    using enum gfx::PixelFormat;
    switch (pixel_format) {
    case DXT1:
        return 8;
    case DXT3:
        return 16;
    case DXT5:
        return 16;
    case ATI1:
        return 8;
    case ATI2:
        return 16;
    case BGR:
        return 48;
    case BGRA:
        return 64;
    case BPTC_RGB_SFLOAT:
        return 16;
    case BPTC_RGB_UFLOAT:
        return 16;
    }

    MG_ASSERT(false && "Unexpected value for gfx::PixelFormat");
}

/** Get the number of blocks in a mipmap of given dimensions. */
size_t num_blocks_by_img_size(uint32_t width, uint32_t height)
{
    return ((width + 3u) / 4) * ((height + 3u) / 4);
}

size_t num_blocks_in_nth_mipmap(size_t n, uint32_t width, uint32_t height)
{
    width = max(1u, width >> n);
    height = max(1u, height >> n);
    return num_blocks_by_img_size(width, height);
}

size_t num_blocks_in_image(size_t num_mip_levels, uint32_t width, uint32_t height)
{
    size_t result = 0;
    for (size_t i = 0; i < num_mip_levels; ++i) {
        result += num_blocks_in_nth_mipmap(i, width, height);
    }
    return result;
}

bool is_cubemap(const DDS_HEADER& header)
{
    return (header.dwCaps2 & DDSCAPS2_CUBEMAP) != 0;
}

bool has_all_cubemap_faces(const DDS_HEADER& header)
{
    return (header.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX) != 0 &&
           (header.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) != 0 &&
           (header.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY) != 0 &&
           (header.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) != 0 &&
           (header.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) != 0 &&
           (header.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ) != 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// TextureResource implementation
//--------------------------------------------------------------------------------------------------

LoadResourceResult TextureResource::load_resource_impl(ResourceLoadingInput& input)
{
    std::span<const std::byte> dds_data = input.resource_data();

    auto try_read_to = [&dds_data]<typename T>(T& destination) -> bool {
        static_assert(std::is_trivially_copyable_v<T>);
        if (dds_data.size() < sizeof(T)) {
            return false;
        }
        std::memcpy(&destination, dds_data.data(), sizeof(T));
        dds_data = dds_data.subspan(sizeof(T));
        return true;
    };

    auto missing_data = [](const std::string& where) {
        return LoadResourceResult::data_error("DDS file corrupt. Missing data: " + where);
    };

    // Read file-type identifier four-character code
    uint32_t four_cc{};
    if (!try_read_to(four_cc)) {
        return missing_data("FourCC");
    }

    if (four_cc != make_fourcc("DDS ")) {
        return LoadResourceResult::data_error("File does not contain DDS image data.");
    }

    // Read header
    DDS_HEADER header{};
    if (!try_read_to(header)) {
        return missing_data("DDS_HEADER");
    }

    // Read extended DX10 header, if present.
    Opt<DDS_HEADER_DXT10> dx10_header = nullopt;
    if (header.ddspf.dwFourCC == make_fourcc("DX10")) {
        dx10_header.emplace();
        if (!try_read_to(dx10_header.value())) {
            return missing_data("DDS_HEADER_DXT10");
        }
    }

    // Determine texture's pixel format
    gfx::PixelFormat pixel_format = gfx::PixelFormat::DXT1;

    // Get number of images, in case the file contains an array
    auto get_image_array_size = [](const DDS_HEADER_DXT10& dx10_header) {
        return dx10_header.arraySize;
    };
    const auto image_array_size = dx10_header.map_or(get_image_array_size, 1u);

    if (dx10_header.has_value()) {
        // The newer way: read the DX10 header.
        if (dx10_header->resourceDimension != D3D10_RESOURCE_DIMENSION_TEXTURE2D) {
            return LoadResourceResult::data_error("Only 2D textures are currently supported.");
        }

        switch (dx10_header->dxgiFormat) {
        case DXGI_FORMAT_BC6H_SF16:
            pixel_format = gfx::PixelFormat::BPTC_RGB_SFLOAT;
            break;
        case DXGI_FORMAT_BC6H_UF16:
            pixel_format = gfx::PixelFormat::BPTC_RGB_UFLOAT;
            break;
        default:
            return LoadResourceResult::data_error(
                fmt::format("Unsupported DXGI_FORMAT: 0x{:x}",
                            as<uint32_t>(dx10_header->dxgiFormat)));
        }
    }
    else {
        // The traditional way.
        auto result = dds_pf_to_pixel_format(header.ddspf);
        if (!result.valid) {
            return LoadResourceResult::data_error(
                fmt::format("Unsupported DDS format: {}", decompose_fourcc(header.ddspf.dwFourCC)));
        }
        pixel_format = result.format;
    }

    if (is_cubemap(header) && !has_all_cubemap_faces(header)) {
        return LoadResourceResult::data_error("File is a cube map, but does not have six faces.");
    }

    // Size sanity check
    const auto width = header.dwWidth;
    const auto height = header.dwHeight;
    const auto num_mip_maps = header.dwMipMapCount;
    const auto num_images = (is_cubemap(header) ? 6u : 1u) * image_array_size;

    // Count size for all mipmaps
    const size_t num_blocks = num_blocks_in_image(num_mip_maps, width, height);

    // The expected size of the image data. Actually an upper bound, since non-block-compressed
    // textures may not be evenly divisible by the block size; as a result, num_blocks_by_img_size
    // may have rounded up the number of blocks.
    const auto expected_size = num_images * num_blocks * block_size_by_format(pixel_format);

    // TODO if the above comment is correct, then this may fire on non-block-compressed DDS files of
    // dimensions not divisible by block size.
    if (dds_data.size() < expected_size) {
        return missing_data("image data");
    }

    if (dds_data.size() > expected_size) {
        const auto fname = resource_id().c_str();
        log.warning(
            "TextureResource '{}': file has different length than "
            "expected given its header data, and is possibly corrupt.",
            fname);
    }

    // Set up texture format info struct
    m_format.pixel_format = pixel_format;
    m_format.width = width;
    m_format.height = height;
    m_format.mip_levels = num_mip_maps;
    m_format.num_images = num_images;

    // DDS files without mipmapping (sometimes?) claim to have 0 mipmaps,
    // but for consistency we always count the base image as a mipmap.
    if (m_format.mip_levels == 0) {
        m_format.mip_levels = 1;
    }

    // Copy pixel data
    m_pixel_data = Array<std::byte>::make_copy(dds_data);

    return LoadResourceResult::success();
}

TextureResource::MipLevelData TextureResource::pixel_data(uint32_t mip_index,
                                                          uint32_t image_index) const noexcept
{
    MG_ASSERT(image_index < m_format.num_images);
    MG_ASSERT(mip_index < m_format.mip_levels);

    const auto block_size = block_size_by_format(m_format.pixel_format);

    // Accumulated size of all mipmaps in one image.
    const auto image_size =
        num_blocks_in_image(m_format.mip_levels, m_format.width, m_format.height) * block_size;

    // Find offset and size of desired mip map.
    auto width = m_format.width;
    auto height = m_format.height;
    size_t offset = as<size_t>(image_index) * image_size;
    auto size = num_blocks_by_img_size(width, height) * block_size;
    {
        for (uint32_t i = 0; i < mip_index; ++i) {
            offset += size;
            width = max(1u, width / 2);
            height = max(1u, height / 2);
            size = num_blocks_by_img_size(width, height) * block_size;
        }
    }

    MG_ASSERT(m_pixel_data.size() >= offset + size);
    return MipLevelData{ std::span(m_pixel_data).subspan(offset, size), width, height };
}

} // namespace Mg
