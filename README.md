# Mg Engine
_Hobby project game engine / rendering library. Very much a work-in-progress._

## Current state
It is currently more of a library of rendering and game engine utilities than a self-contained
framework. Most interesting, perhaps, is the implementation of cluster forward rendering for point
lights (see include/mg/gfx/mg\_lit\_mesh\_renderer).

There are a lot of experimental features and systems are being continuously re-designed.
Since this is a hobby project, there is no clear roadmap -- I just work on what I find interesting.

## Supported Platforms
Mg Engine is developed and tested on

* Linux
	* GCC 8.1.0
	* Clang 6.0
* Windows
	* Visual Studio 2017 (N.B. build only occasionally tested / fixed)

Mg Engine might or might not work on other platforms or compilers.

## Building
Mg Engine uses CMake for building.
If you have used CMake before, you know what to do.
**TODO** add more specific build instructions for different platforms.

## License
Mg Engine is zlib-licensed.

