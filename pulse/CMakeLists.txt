cmake_minimum_required(VERSION 3.18)

project(pulse-headers LANGUAGES C VERSION 17.0)

set(HEADERS 
    cdecl.h
    channelmap.h
    context.h
    def.h
    direction.h
    error.h
    ext-device-manager.h
    ext-device-restore.h
    ext-stream-restore.h
    format.h
    gccmacro.h
    glib-mainloop.h
    introspect.h
    mainloop-api.h
    mainloop-signal.h
    mainloop.h
    operation.h
    proplist.h
    pulseaudio.h
    rtclock.h
    simple.h
    stream.h
    subscribe.h
    thread-mainloop.h
    timeval.h
    utf8.h
    util.h
    version.h
    volume.h
    xmalloc.h)

add_library(pulse-headers INTERFACE)

target_sources(pulse-headers INTERFACE ${HEADERS})

target_include_directories(pulse-headers INTERFACE ${ROOT})
target_include_directories(pulse-headers INTERFACE ${glib_INCLUDE_DIR})
