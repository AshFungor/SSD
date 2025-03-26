# Sound Server Daemon and his best friend Pulse Calls Mapper

## Overview

Sound Server Daemon is laar's substitute for pulseaudio daemon in our systems. It 
uses PCM (Pulse Calls Mapper) to map pulseaudio API to our own and pack requests
in messages for SSD (Sound Server Daemon) to process them.

## Setup (for x64_86)

### Overview and container build

Server and adaption layer support multiple processor architectures, including x64_86 and arm64v8 (Probably more - did not compile yet).
To build for x64_86, you can use local environment with instructions below, or build and run environment in docker.
Image is located under ```containers/x64_86/```. You should mount ```/home/runner/project/build/``` directory to local
build directory in your filesystem.

### Alternative build in user setup

Make sure you have conan (with python support, version >= 2), cmake, cxx and c
compiler in your system.

Dependencies:
- glibc
- protobuf
- gtest
- plog

Setting up protobuf might be tricky. Currently build solution is able to 
correctly handle system setup with protoc and protobuf headers installed in
system paths, but I still recommend to avoid external protobuf anyway.
Build script in scripts/ dir is sufficient for cmake building/configuring.
To build, use ```build.sh -b``` command, to configure: ```build.sh -c```

My system setup (conan profile):

```
[settings]
arch=x86_64
build_type=Debug
compiler=gcc
compiler.cppstd=gnu20
compiler.libcxx=libstdc++11
compiler.version=13.2
os=Linux
```

I sincerely hope it works for other setups as well.

## Progress

### 20, Feb 

After glimpsing API of pulseaudio I have found out that it supports simple
transmission of recording/output for its clients, which is perfect because
this sounds fairly easy. So, for next sprint (till mid of March) my task is:

- setup thread pool on server and make it handle simple requests
- allow pcm to correctly handle simple.h

This is all for now.

### 15, June

Main job about server is done, core features are implemented and now I need to progress with
pulseaudio client-side implementation - pcm. I should also fix problems with load-balancing on
sync protocol - on client side. 

### End of development

Unfortunately, the last version of protocol has disasterous performance. If work on this project continues, major refactoring should be done.

