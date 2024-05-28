FROM archlinux:base-devel

VOLUME /project/build

RUN pacman -Syu --noconfirm

# cross compilation deps
RUN pacman -S --noconfirm aarch64-linux-gnu-linux-api-headers   \
                          aarch64-linux-gnu-gcc                 \
                          aarch64-linux-gnu-glibc               \
                          aarch64-linux-gnu-binutils

# deps CPP manager
RUN pacman -S --noconfirm python python-pip
RUN pip install --break-system-packages --no-cache conan

RUN pacman -S --noconfirm cmake

# clear pacman cache
RUN pacman -S --noconfirm pacman-contrib
RUN paccache -r -k 0

# set env vars for gcc toolchain
ENV CC aarch64-linux-gnu-gcc
ENV CXX aarch64-linux-gnu-g++
ENV AR aarch64-linux-gnu-ar
ENV AS aarch64-linux-gnu-as
ENV FC aarch64-linux-gnu-gfortran
ENV LD aarch64-linux-gnu-ld
# system settings
ENV ARCH arm64

COPY . /project
WORKDIR /project

RUN conan profile detect
RUN mv ./data/arm64.ini /root/.conan2/profiles/host
ENV CONAN_ARGS --profile:host=host

RUN conan install . --build=missing $CONAN_ARGS

CMD ./scripts/build.sh -c && ./scripts/build.sh -b