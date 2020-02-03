//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_file_time_helper.h
 * Helper functions to deal with portability issues with file time stamps.
 */

#pragma once

#include <ctime>
#include <filesystem>

namespace Mg {

// Get last write time of file as time_t (more portable than the unspecified time type guaranteed by
// C++17 standard).
std::time_t last_write_time_t(const std::filesystem::path& file);

} // namespace Mg
