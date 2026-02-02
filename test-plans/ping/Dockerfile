FROM alpine:3.19 AS builder

RUN apk add --no-cache \
    git \
    cmake \
    ninja \
    curl \
    zip \
    unzip \
    tar \
    pkgconf \
    gcc \
    g++ \
    nasm \
    linux-headers \
    musl-dev \
    make \
    bash

ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT \
    && $VCPKG_ROOT/bootstrap-vcpkg.sh \
    && rm -rf $VCPKG_ROOT/.git

WORKDIR /build
COPY . .

RUN cmake --preset test-plans -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_EXE_LINKER_FLAGS="-static" \
    && cmake --build build -j$(nproc) --target ping

FROM alpine:3.19

COPY --from=builder /build/build/test-plans/ping/ping /ping

ENTRYPOINT ["/ping"]