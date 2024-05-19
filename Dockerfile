FROM alpine:latest as build_env

RUN apk update
RUN apk add gcc cmake python3 py3-pip

RUN pip3 install --break-system-packages conan2

COPY . /project-root/
WORKDIR /project-root

RUN /project-root/scripts/build.sh -c && /project-root/scripts/build.sh -b

CMD ./build/src/ssd/sound-server-daemon
