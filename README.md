# Mg Engine
_Early work on an OpenGL-based rendering/game engine_

## Current state
Can render simple scenes, when given very explicit instructions. Next step is to improve renderer
and create a higher-level scene system, to allow simpler representation of scenes.

## Supported Platforms
Mg Engine is developed and tested on

* Linux
	* GCC 5.3.0 and later
	* Clang 3.9 and later
* Windows
	* Visual Studio 2015 and later

Mg Engine might or might not work on other platforms or compilers.

## Building
Mg Engine uses CMake for building.
If you have used CMake before, you know what to do.
**TODO** add more specific build instructions for different platforms.

**Note**: Mg Engine relies on the filesystem TS, which is available with the aforementioned
compilers, however, you may need to explicitly link it. With GCC or Clang using libstdc++, this is
done by adding `-lstdc++fs` to the linker options. If you build using the provided CMakeLists, this
is handled automatically.

## License
Mg Engine is zlib-licensed.

