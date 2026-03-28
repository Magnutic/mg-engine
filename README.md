# Mg Engine
Hobby project game engine in C++20

## Building
Builds with CMake.

First, make sure the dependencies are available: see subsection _Dependencies_ below.
Then, build using CMake:

```cpp
cmake -S . -B build-directory <options...>
cmake --build build-directory -j $(nproc)
```

## Dependencies

Dependencies are bundled as submodules (`external/submodules/`).

> On Debian-based systems, building the dependencies requires the following packages to be installed:
>
> `apt install -y libwayland-dev libxkbcommon-dev xorg-dev`

They can be built as follows:

```sh
cmake -P ./cmake/build_dependencies.cmake
```

Optionally, you can specify compiler to use for building dependencies:

```sh
CC=clang-20 CXX=clang++-20 cmake -P ./cmake/build_dependencies.cmake
```

For convenience, you can also let the build system build the  dependencies automatically, when
configuring the project:

```sh
cmake -S . -B build-directory -DMG_ENGINE_BUILD_DEPENDENCIES=1
```

## License
Mg Engine is licensed under the BSD 3-clause license, see LICENSE.txt.

