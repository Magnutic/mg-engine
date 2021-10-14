# Curbe editor GUI application for Mg Engine
cmake_minimum_required(VERSION 3.15)

add_executable(curve_editor curve_editor_app.cpp)
target_link_libraries(curve_editor mg_engine mg_tools_shared)
