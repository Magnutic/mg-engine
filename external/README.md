# External, third-party dependencies

This directory holds source code for third-party libraries used by Mg Engine.

The libraries are stored in the archive `mg_dependencies.zip`. There is also a submodules directory,
but it is only used to generate the dependencies archive (see below).

To update a third-party library, go to its directory under `submodules` and `git checkout` the
desired revision. Then run `update_dependency_archive.sh` to update `mg_dependencies.zip` with the
new version.
