# External, third-party dependencies

This directory holds source code for third-party libraries used by Mg Engine.

There is also a submodules directory which can be used to fetch and build dependencies.
Alternatively, it can be used to create an archive containing all dependencies (see below).

To update a third-party library, go to its directory under `submodules` and `git checkout` the
desired revision.

To create a new dependency archive, run `cmake -P cmake/generate_dependency_archive.cmake` in the
Mg Engine project root directory to generate `mg_dependencies_src.zip` containing the current
revisions of all submodules.
