# Mg Engine

Game engine project which I have been tinkering with on and off for a while.
As of writing, it uses C++20 and OpenGL 4.4.

It contains a renderer using _Clustered Forward Rendering_ [1].

[1] <https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf>

## Building

Build using GCC or Clang using CMake as build-system generator.

In short, for Debian-based systems:

```sh
apt install -y build-essential cmake libwayland-dev libxkbcommon-dev xorg-dev
cmake -P ./cmake/build_dependencies.cmake
cmake -S . -B mybuild
cmake --build mybuild -j $(nproc)
ctest --test-dir mybuild  # Run unit tests
./mybuild/bin/test_scene  # Launch sample application
```

> MSVC was once supported, but it has been a long time since I used Windows.
> It may well be possible to build under Windows using MinGW or similar.
> Supporting Windows builds again is on the roadmap.

### In more detail:

First, make sure the dependencies are available: see section _Dependencies_ below.
Then, build using CMake:

```cpp
cmake -S . -B build-directory <options...>
cmake --build build-directory -j $(nproc)
```

Refer to CMakeLists.txt for the available build options.

There are also utility scripts under the `build/` directory that invoke CMake for you.
It is not mandatory to use these.

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

For convenience, you can also let the build system build the dependencies automatically, when
configuring the project:

```sh
cmake -S . -B build-directory -DMG_BUILD_DEPENDENCIES=1
```

## License

Mg Engine is licensed under the BSD 3-clause license, see LICENSE.txt.
