FROM ogarcia/archlinux:devel

RUN pacman -Syu --noconfirm

# deps CPP manager
RUN pacman -S --noconfirm python python-pip
RUN pip install --break-system-packages --no-cache conan

RUN pacman -S --noconfirm cmake gcc glibc make alsa-lib

# clear pacman cache
RUN pacman -S --noconfirm pacman-contrib
RUN paccache -r -k 0

RUN groupadd runner && useradd -g runner runner && chmod 755 /home
USER runner
WORKDIR /home/runner

RUN mkdir project
RUN mkdir -p ~/.conan2/profiles

COPY --chown=runner conanfile.py project/conanfile.py
COPY --chown=runner data/debug.ini /home/runner/.conan2/profiles/default
WORKDIR /home/runner/project

RUN conan install . --build=missing

CMD ["./scripts/build.py", "-c", "-b", "--config", "Debug", "--dir", "container_build"]