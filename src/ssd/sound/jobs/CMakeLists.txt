cmake_minimum_required(VERSION 3.15)

project(sound LANGUAGES CXX VERSION 0.0)

set(SOURCES async-dispatching-job.cpp)
set(HEADERS async-dispatching-job.hpp)

add_library(jobs STATIC ${SOURCES} ${HEADERS})

target_include_directories(jobs PUBLIC ${SRC_ROOT})

target_link_libraries(jobs PUBLIC common)
target_link_libraries(jobs PUBLIC dispatchers)
target_link_libraries(jobs PUBLIC fftw::fftw)