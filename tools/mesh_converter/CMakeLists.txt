# Mesh converter for Mg Engine
cmake_minimum_required(VERSION 3.15)

add_executable(mesh_converter auto_mesh_converter.cpp mg_mesh_converter.cpp)
target_link_libraries(mesh_converter mg_engine mg_tools_shared assimp::assimp)
mg_set_output_directory(mesh_converter)
