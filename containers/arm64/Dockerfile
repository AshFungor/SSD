FROM archlinux:base-devel

RUN pacman -Syu --noconfirm

# cross compilation deps
RUN pacman -S --noconfirm aarch64-linux-gnu-linux-api-headers   \
                          aarch64-linux-gnu-gcc                 \
                          aarch64-linux-gnu-glibc               \
                          aarch64-linux-gnu-binutils

# deps CPP manager
RUN pacman -S --noconfirm python python-pip
RUN pip install --break-system-packages --no-cache conan

RUN pacman -S --noconfirm cmake gcc make

# clear pacman cache
RUN pacman -S --noconfirm pacman-contrib
RUN paccache -r -k 0

RUN groupadd runner && useradd -g runner runner && chmod 755 /home
USER runner
WORKDIR /home/runner

RUN mkdir project
COPY --chown=runner conanfile.py project/conanfile.py
COPY --chown=runner data/arm64.ini project/arm64.ini
WORKDIR /home/runner/project

RUN conan profile detect
RUN cp arm64.ini ~/.conan2/profiles/host
ENV CONAN_ARGS --profile:host=host

RUN conan install . --build=missing $CONAN_ARGS

# system settings
ENV ARCH arm64

# set env vars for gcc toolchain
ENV CC aarch64-linux-gnu-gcc
ENV CXX aarch64-linux-gnu-g++
ENV AR aarch64-linux-gnu-ar
ENV AS aarch64-linux-gnu-as
ENV FC aarch64-linux-gnu-gfortran
ENV LD aarch64-linux-gnu-ld

CMD ./scripts/build.py -c -b --config Release --link