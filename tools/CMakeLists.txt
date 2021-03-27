# Tools for mg-engine.
cmake_minimum_required(VERSION 3.15)

add_executable(mesh_converter auto_mesh_converter.cpp mg_mesh_converter.cpp mg_file_writer.cpp)
target_link_libraries(mesh_converter mg_engine assimp::assimp)
