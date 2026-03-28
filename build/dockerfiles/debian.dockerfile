ARG APT_PACKAGES=""

# Build dependencies
FROM debian:trixie-slim AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update \
    && apt-get install -y build-essential git cmake libwayland-dev libxkbcommon-dev xorg-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*
COPY . /src
WORKDIR /src
RUN cmake -DMG_DEPENDENCIES_INSTALL_DIR=/opt/mg_deps -DMG_DEPENDENCIES_FETCH_SUBMODULES=0 -P cmake/build_dependencies.cmake

# Build image with dependencies installed
FROM debian:trixie-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update \
    && apt-get install -y build-essential git cmake libwayland-dev libxkbcommon-dev xorg-dev clang clang-tidy \
    && apt-get clean && rm -rf /var/lib/apt/lists/*
COPY --from=builder /opt/mg_deps /opt/mg_deps
CMD ["/bin/bash"]
