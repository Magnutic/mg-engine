import os

flags = [
    '-Wall',
    '-Wextra',
    '-Wdeprecated',
    '-Wshadow',
    '-Wnon-virtual-dtor',
    '-Wconversion',
    '-std=c++20',
    '-x', 'c++',
    '-I', './external/mg_dependencies/include',
    '-I', './external/mg_dependencies/include/bullet',
    '-I', './external/mg_dependencies/function2/include',
    '-I', './external/mg_dependencies/optional/include',
    '-I', './external/mg_dependencies/plf_colony',
    '-I', './external/mg_dependencies/stb',
    '-I', './external/mg_dependencies/imgui',
    '-I', './external/mg_dependencies/utf8.h',
    '-I', './external/glad/include',
    '-I', './include',
    '-I', './src',
    '-I', './build/deps/include',
    '-isystem', '/usr/local/include',
    '-DGLM_FORCE_XYZW_ONLY=1'
]

def DirectoryOfThisScript():
    return os.path.dirname(os.path.abspath(__file__))

def Settings(filename, **kwargs ):
    return {
        'flags': flags,
        'include_paths_relative_to_dir': DirectoryOfThisScript()
    }

