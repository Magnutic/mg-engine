# Build helper scripts

The scripts in this directory are provided as a convenience for building. They are not mandatory; it
is possible to configure and build mg-engine using regular CMake conventions -- these scripts are
just wrappers that invoke CMake. But they simplify configuration and building, and demonstrate some
of mg-engine's build options.

## Usage

First, run `configure.sh` with your compiler(s) of choice, e.g.:

    ./configure.sh clang_debug clang clang++ --debug
    ./configure.sh gcc_release gcc g++

This will create two build configurations, titled `clang_debug` in `out/clang_debug/` and
`gcc_release`. The first time it is run, it will also compile all dependencies at put them in
`out/mg_dependencies`.

Run `configure.sh` without any arguments for information on available options.

The `build_and_run.sh <configuration> <build-target>` script is a convenience for building and
running a given configuration. For example, using the `clang_debug` configuration from the previous
example:

    ./build_and_run.sh clang_debug test_scene

There is also a `build.sh` with the same interface as `build_and_run.sh`.

## For Windows builds

These scripts should work under an environment like Cygwin or MSYS2. Otherwise, you can configure
the build for Visual Studio directly using CMake.

