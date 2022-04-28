//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg_file_writer.h"

#include "mg/core/mg_log.h"

#include <fstream>

namespace Mg {

namespace {
uint32_t get_current_pos(std::ofstream& stream)
{
    const auto pos = stream.tellp();
    stream.seekp(0);
    const auto filestart = stream.tellp();
    stream.seekp(pos);
    return static_cast<uint32_t>(pos - filestart);
}
} // namespace

FileDataRange FileWriter::enqueue(const char* data, uint32_t length)
{
    const uint32_t start = m_enqueued.empty() ? 0 : m_enqueued.back().range.end;
    FileDataRange range = { start, 0 };
    range.end = range.begin + length;
    m_enqueued.push_back({ range, data, length });
    return range;
}

bool FileWriter::write(const std::filesystem::path& out_path)
{
    auto tmp_path = out_path;
    tmp_path.replace_filename(tmp_path.filename().concat("_tmp"));

    std::ofstream out_file;
    out_file.open(tmp_path, std::ios::out | std::ios::binary | std::ios::trunc);

    if (!out_file.is_open() || !out_file.good()) {
        log.error("Failed to write file '{}': could not open for writing.", tmp_path.u8string());
        return false;
    }

    for (const QueuedWrite& item : m_enqueued) {
        if (get_current_pos(out_file) != item.range.begin) {
            log.error("FileWriter: unexpected error. Stream write position does not match queue.");
            return false;
        }
        out_file.write(item.data, as<std::streamsize>(item.length));
    }

    out_file.close();
    std::error_code ec;
    std::filesystem::rename(tmp_path, out_path, ec);

    if (ec) {
        log.error("Failed to write file '{}': {}", out_path.u8string(), ec.message());
        return false;
    }

    log.message("Wrote file '{}'.", out_path.u8string());
    return true;
}

} // namespace Mg
