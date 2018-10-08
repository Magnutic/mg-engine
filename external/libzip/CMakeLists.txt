# This file is originally from libzip, with copyright and license as specified in LICENSE.
# Modified for Mg Engine.

cmake_minimum_required(VERSION 3.1)

project(libzip C)

include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCSourceRuns)
include(CheckCSourceCompiles)
include(CheckStructHasMember)
include(TestBigEndian)

# Checks

check_function_exists(_close HAVE__CLOSE)
check_function_exists(_dup HAVE__DUP)
check_function_exists(_fdopen HAVE__FDOPEN)
check_function_exists(_fileno HAVE__FILENO)
check_function_exists(_open HAVE__OPEN)
check_function_exists(_setmode HAVE__SETMODE)
check_function_exists(_snprintf HAVE__SNPRINTF)
check_function_exists(_strdup HAVE__STRDUP)
check_function_exists(_stricmp HAVE__STRICMP)
check_function_exists(_strtoi64 HAVE__STRTOI64)
check_function_exists(_strtoui64 HAVE__STRTOUI64)
check_function_exists(explict_bzero HAVE_EXPLICIT_BZERO)
check_function_exists(explicit_memset HAVE_EXPLICIT_MEMSET)
check_function_exists(fileno HAVE_FILENO)
check_function_exists(fseeko HAVE_FSEEKO)
check_function_exists(ftello HAVE_FTELLO)
check_function_exists(getprogname HAVE_GETPROGNAME)
check_function_exists(open HAVE_OPEN)
check_function_exists(mkstemp HAVE_MKSTEMP)
check_function_exists(setmode HAVE_SETMODE)
check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(stricmp HAVE_STRICMP)
check_function_exists(strtoll HAVE_STRTOLL)
check_function_exists(strtoull HAVE_STRTOULL)

check_include_files(fts.h HAVE_FTS_H)
check_include_files(stdbool.h HAVE_STDBOOL_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(unistd.h HAVE_UNISTD_H)

check_include_files(inttypes.h HAVE_INTTYPES_H_LIBZIP)
check_include_files(stdint.h HAVE_STDINT_H_LIBZIP)
check_include_files(sys/types.h HAVE_SYS_TYPES_H_LIBZIP)

check_type_size(__int8 __INT8_LIBZIP)
check_type_size(int8_t INT8_T_LIBZIP)
check_type_size(uint8_t UINT8_T_LIBZIP)
check_type_size(__int16 __INT16_LIBZIP)
check_type_size(int16_t INT16_T_LIBZIP)
check_type_size(uint16_t UINT16_T_LIBZIP)
check_type_size(__int32 __INT32_LIBZIP)
check_type_size(int32_t INT32_T_LIBZIP)
check_type_size(uint32_t UINT32_T_LIBZIP)
check_type_size(__int64 __INT64_LIBZIP)
check_type_size(int64_t INT64_T_LIBZIP)
check_type_size(uint64_t UINT64_T_LIBZIP)
check_type_size("short" SHORT_LIBZIP)
check_type_size("int" INT_LIBZIP)
check_type_size("long" LONG_LIBZIP)
check_type_size("long long" LONG_LONG_LIBZIP)
check_type_size("off_t" SIZEOF_OFF_T)
check_type_size("size_t" SIZE_T_LIBZIP)
check_type_size("ssize_t" SSIZE_T_LIBZIP)

test_big_endian(WORDS_BIGENDIAN)

set(CMAKE_REQUIRED_INCLUDES ${ZLIB_INCLUDE_DIR})

if(MSVC)
add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif(MSVC)

add_definitions("-DHAVE_CONFIG_H")

# Targets
add_subdirectory(lib)

# write out config file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake-zipconf.h.in ${CMAKE_CURRENT_BINARY_DIR}/zipconf.h)

# installation
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zipconf.h DESTINATION ${MG_HEADER_INSTALL_PATH})
