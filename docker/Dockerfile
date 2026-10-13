# Pinned Debian 12 (Bookworm) base image for reproducible build
FROM debian:12-slim@sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install compiler, build tools, X11, OpenGL, audio, and dev dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ninja-build \
    git \
    curl \
    ca-certificates \
    libx11-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxi-dev \
    libudev-dev \
    libgl1-mesa-dev \
    libfreetype-dev \
    libharfbuzz-dev \
    libvorbis-dev \
    libflac-dev \
    libogg-dev \
    libsqlite3-dev \
    patchelf \
    zip \
    python3 \
    binutils \
    file \
    && rm -rf /var/lib/apt/lists/*

# Install modern CMake (3.28.3) required by SFML 3.1.0
RUN curl -sSL https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-linux-x86_64.tar.gz \
    | tar -xz --strip-components=1 -C /usr/local

# Build and install SFML 3.1.0 from source
RUN curl -sSL https://github.com/SFML/SFML/archive/refs/tags/3.1.0.tar.gz | tar -xz \
    && cd SFML-3.1.0 \
    && cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_SHARED_LIBS=ON \
        -DSFML_BUILD_NETWORK=OFF \
        -DSFML_BUILD_EXAMPLES=OFF \
        -DSFML_BUILD_DOC=OFF \
        -DSFML_BUILD_TEST_SUITE=OFF \
    && cmake --build build \
    && cmake --install build \
    && ldconfig \
    && cd .. \
    && rm -rf SFML-3.1.0

# Set up build working directory
WORKDIR /src

# Copy project source files
COPY . .

# Build the arcade version: Energy_Crisis (with -DARCADE_MODE=1)
RUN make clean || true
RUN make TARGET=Energy_Crisis SFML_DIR=/usr/local

# Package the portable arcade release and validate GLIBC inside container
RUN chmod +x /src/scripts/package_inside_docker.sh /src/scripts/check_glibc_symbols.py /src/launch.sh \
    && /src/scripts/package_inside_docker.sh
