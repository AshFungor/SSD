FROM debian:bookworm-slim

RUN apt update && apt install -y --no-install-recommends            \
    gcc-aarch64-linux-gnu                                           \
    g++-aarch64-linux-gnu                                           \
    libc6-dev-arm64-cross                                           \
    linux-libc-dev-arm64-cross                                      \
    binutils-aarch64-linux-gnu                                      \
    perl

RUN dpkg --add-architecture arm64
RUN apt update && apt install -y libasound2-dev:arm64

RUN apt install -y --no-install-recommends                           \
    python3 python3-pip cmake make gcc g++                          \
    && pip3 install --break-system-packages --no-cache-dir conan

RUN apt clean && rm -rf /var/lib/apt/lists/*

RUN groupadd runner && useradd -g runner runner && chmod 755 /home
USER runner
WORKDIR /home/runner

RUN mkdir project
COPY conanfile.py project/conanfile.py
COPY data/conan-configs/debug.ini project/debug.ini
WORKDIR /home/runner/project

RUN conan profile detect
RUN mv debug.ini /home/runner/.conan2/profiles/host
ENV CONAN_ARGS=--profile:host=host

RUN conan install . --build=missing ${CONAN_ARGS}

ENV ARCH=arm64
ENV CC=aarch64-linux-gnu-gcc
ENV CXX=aarch64-linux-gnu-g++
ENV AR=aarch64-linux-gnu-ar
ENV AS=aarch64-linux-gnu-as
ENV FC=aarch64-linux-gnu-gfortran
ENV LD=aarch64-linux-gnu-ld

CMD ["./scripts/build.py", "-c", "-b", "--config", "Debug"]
