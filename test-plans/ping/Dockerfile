# Build stage
FROM ubuntu:24.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    software-properties-common \
    && add-apt-repository ppa:ubuntu-toolchain-r/test \
    && apt-get update && apt-get install -y \
    git \
    cmake \
    ninja-build \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    gcc-14 \
    g++-14 \
    nasm \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-14 100 \
    && rm -rf /var/lib/apt/lists/*

# Set up vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT \
    && $VCPKG_ROOT/bootstrap-vcpkg.sh \
    && rm -rf $VCPKG_ROOT/.git

# Copy project files
WORKDIR /build
COPY . .

# Configure and build
RUN cmake --preset test-plans \
    && cmake --build build -j$(nproc) --target ping

FROM ubuntu:24.04

# Install only essential runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy the compiled binary
COPY --from=builder /build/build/test-plans/ping/ping /usr/local/bin/ping

# Run the ping executable
ENTRYPOINT ["/usr/local/bin/ping"]