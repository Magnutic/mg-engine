//-----------------------------------------------------------------------------
// Mesh converter utility for Mg Engine
//-----------------------------------------------------------------------------

#pragma once

#include <filesystem>

namespace Mg
{

bool convert_mesh(const std::filesystem::path& path_in, const std::filesystem::path& path_out);

} // namespace Mg