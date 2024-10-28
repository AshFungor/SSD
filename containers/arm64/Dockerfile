FROM debian:bookworm-slim

# Update and install cross-compilation dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    libc6-dev-arm64-cross \
    linux-libc-dev-arm64-cross \
    binutils-aarch64-linux-gnu

# Install dependencies for Conan and C++ management
RUN apt-get install -y --no-install-recommends \
    python3 python3-pip cmake make gcc g++ \
    && pip3 install --break-system-packages --no-cache-dir conan

# Clean up apt cache to reduce image size
RUN apt-get clean && rm -rf /var/lib/apt/lists/*

# Create and set up non-root user
RUN groupadd -r runner && useradd -r -g runner runner && mkdir -p /home/runner && chown runner:runner /home/runner
USER runner
WORKDIR /home/runner

# Set up project directory and copy necessary files
RUN mkdir project
COPY --chown=runner conanfile.py project/conanfile.py
COPY --chown=runner data/arm64.ini project/arm64.ini
WORKDIR /home/runner/project

# Configure Conan profile for cross-compilation
RUN conan profile detect
RUN cp arm64.ini ~/.conan2/profiles/host
ENV CONAN_ARGS=--profile:host=host

# Install dependencies with Conan
RUN conan install . --build=missing $CONAN_ARGS

# Set environment variables for GCC cross-compilation toolchain
ENV ARCH arm64
ENV CC=aarch64-linux-gnu-gcc
ENV CXX=aarch64-linux-gnu-g++
ENV AR=aarch64-linux-gnu-ar
ENV AS=aarch64-linux-gnu-as
ENV FC=aarch64-linux-gnu-gfortran
ENV LD=aarch64-linux-gnu-ld

# Set the default command to build the project
CMD ./scripts/build.py -c -b --config Debug --link