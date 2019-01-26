#------------------------------------------------------------------------------
# Mg Engine: from https://github.com/facebook/hhvm
# Modifications: formatting, add IMPORTED target for convenience.
# ZEND/PHP license (see above repo).
#------------------------------------------------------------------------------

# Finds libzip.
#
# This module defines:
# LIBZIP_INCLUDE_DIR_ZIP
# LIBZIP_INCLUDE_DIR_ZIPCONF
# LIBZIP_LIBRARY
#
# And creates imported target:
# zip
#

find_package (PkgConfig)
pkg_check_modules (PC_LIBZIP QUIET libzip)

find_path (LIBZIP_INCLUDE_DIR_ZIP
    NAMES zip.h
    HINTS ${PC_LIBZIP_INCLUDE_DIRS}
)

find_path (LIBZIP_INCLUDE_DIR_ZIPCONF
    NAMES zipconf.h
    HINTS ${PC_LIBZIP_INCLUDE_DIRS}
)

find_library (LIBZIP_LIBRARY NAMES zip)

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (
    libzip DEFAULT_MSG
    LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR_ZIP LIBZIP_INCLUDE_DIR_ZIPCONF
)

set(LIBZIP_VERSION 0)

if (LIBZIP_INCLUDE_DIR_ZIPCONF)
    file (READ "${LIBZIP_INCLUDE_DIR_ZIPCONF}/zipconf.h" _LIBZIP_VERSION_CONTENTS)
    if (_LIBZIP_VERSION_CONTENTS)
        string (REGEX REPLACE ".*#define LIBZIP_VERSION \"([0-9.]+)\".*" "\\1" LIBZIP_VERSION "${_LIBZIP_VERSION_CONTENTS}")
    endif()
endif()

set (LIBZIP_VERSION ${LIBZIP_VERSION} CACHE STRING "Version number of libzip")

# Set up imported target
if (LIBZIP_FOUND)
    add_library (zip UNKNOWN IMPORTED)
    set_target_properties (zip PROPERTIES
        IMPORTED_LOCATION ${LIBZIP_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES "${LIBZIP_INCLUDE_DIR_ZIP};${LIBZIP_INCLUDE_DIR_ZIPCONF}"
        IMPORTED_LINK_INTERFACE_LANGUAGES CXX
    )
endif()

