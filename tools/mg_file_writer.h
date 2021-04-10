//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_file_writer.h
 * Utility for writing binary files.
 */

#pragma once

#include <mg/mg_file_data_range.h>
#include <mg/utils/mg_gsl.h>

#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

namespace Mg {

/** Utility class for writing binary files.
 * Important: enqueued data is only stored by reference -- it is the user's responisibility that the
 * data remains in place for the lifetime of the FileWriter.
 */
class FileWriter {
public:
    /** Enqueue a vector of data for writing.
     * @return A FileDataRange describing where in the file the data will be.
     */
    template<typename T> FileDataRange enqueue(const std::vector<T>& items)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        return enqueue(reinterpret_cast<const char*>(items.data()), items.size() * sizeof(T));
    }

    /** Enqueue a struct for writing.
     * @return A FileDataRange describing where in the file the data will be.
     */
    template<typename T> FileDataRange enqueue(const T& item)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        return enqueue(reinterpret_cast<const char*>(&item), sizeof(T));
    }

    /** Enqueue a string for writing.
     * @return A FileDataRange describing where in the file the data will be.
     */
    FileDataRange enqueue(const std::string& string)
    {
        return enqueue(string.c_str(), narrow<uint32_t>(string.size()));
    }

    /** Enqueue arbitrary data for writing.
     * @return A FileDataRange describing where in the file the data will be.
     */
    FileDataRange enqueue(const char* data, uint32_t length);

    /** Perform all enqueued writes.
     * @return Whether write was successful.
     */
    bool write(const std::filesystem::path& out_path);

private:
    struct QueuedWrite {
        FileDataRange range;
        const char* data = nullptr;
        size_t length = 0;
    };

    std::vector<QueuedWrite> m_enqueued;
};

} // namespace Mg
