//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include <cstdint>

namespace Mg {
class TextureResource;
class TextureSettings;
} // namespace Mg

namespace Mg::gfx {

// Texture format info as required by OpenGL
struct GlTextureInfo {
    uint32_t internal_format;
    uint32_t format;
    uint32_t type;
    int32_t mip_levels;
    int32_t width;
    int32_t height;
    float aniso;
    bool compressed;
};

// Get texture format info as required by OpenGL
GlTextureInfo gl_texture_info(const TextureResource& resource,
                              const TextureSettings& settings) noexcept;

// Set up texture sampling parameters for currently bound texture
void set_sampling_params(const TextureSettings& settings) noexcept;


} // namespace Mg::gfx
