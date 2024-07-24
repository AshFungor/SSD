FROM archlinux:base-devel

RUN pacman -Syu --noconfirm

# deps CPP manager
RUN pacman -S --noconfirm python python-pip
RUN pip install --break-system-packages --no-cache conan

RUN pacman -S --noconfirm cmake gcc glibc make

# clear pacman cache
RUN pacman -S --noconfirm pacman-contrib
RUN paccache -r -k 0

RUN groupadd runner && useradd -g runner runner && chmod 755 /home
USER runner
WORKDIR /home/runner

RUN mkdir project
COPY --chown=runner conanfile.py project/conanfile.py
COPY --chown=runner data/x64_86.ini project/x64_86.ini
WORKDIR /home/runner/project

RUN conan profile detect
RUN cp x64_86.ini ~/.conan2/profiles/host
ENV CONAN_ARGS=--profile:host=host

RUN conan install . --build=missing $CONAN_ARGS

ENV BUILD_TYPE=Release

CMD ./scripts/build.py -c -b --config Release --config Debug --link