include(CMakeFindDependencyMacro)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_PREFIX_PATH}/lib/cmake/libzip")

find_dependency(Threads)
find_dependency(libzip 1.6.1)
find_dependency(fmt 6.0)
find_dependency(glfw3 3.3)
find_dependency(glm 0.9.9)

include("${CMAKE_CURRENT_LIST_DIR}/mg_engine_targets.cmake")
