get_filename_component(SELF_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(${SELF_DIR}/mg_engine.cmake)

find_package(Threads REQUIRED)
find_package(glfw3 3.2 REQUIRED)
