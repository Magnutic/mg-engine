# Shared utilities for Mg Engine tools.
cmake_minimum_required(VERSION 3.15)

add_library(mg_tools_shared mg_file_writer.cpp)
target_link_libraries(mg_tools_shared PUBLIC mg_engine)
