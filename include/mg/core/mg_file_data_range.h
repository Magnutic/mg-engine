#pragma once

#include <cstdint>

namespace Mg {

/** Describes where to find a range of data within a binary file. */
struct FileDataRange {
    /** Begin byte offset. */
    uint32_t begin = 0;

    /** End byte offset. */
    uint32_t end = 0;
};

} // namespace Mg
