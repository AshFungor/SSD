cmake_minimum_required(VERSION 3.19)

project(sound-server LANGUAGES CXX C)

include(CTest)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# export compile commands for linting
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add -Wall if possible
if (CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif (CMAKE_COMPILER_IS_GNUCXX)

# packages
find_package(absl CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED VERSION 1.86.0)
find_package(protobuf CONFIG REQUIRED VERSION 5.27.0)
find_package(nlohmann_json CONFIG REQUIRED VERSION 3.11.3)
find_package(GTest CONFIG REQUIRED VERSION 1.14.0)
find_package(plog CONFIG REQUIRED VERSION 1.1.10)
find_package(FFTW3 CONFIG REQUIRED VERSION 3.3.10)

set(SSD_EXTERNAL_DEPS 
	plog::plog 
	fftw::fftw 
	# linking against tests fails linking Google tests, so pick needed components
	# here
	Boost::system Boost::timer Boost::context
	protobuf::libprotobuf
	nlohmann_json::nlohmann_json
)

set(DATA_DIR ${CMAKE_SOURCE_DIR}/data)

# example configs
file(COPY ${DATA_DIR}/app-configs/default.cfg DESTINATION ${CMAKE_BINARY_DIR}/src/ssd)
file(COPY ${DATA_DIR}/app-configs/dynamic.cfg DESTINATION ${CMAKE_BINARY_DIR}/src/ssd)

# contrib libs not on conan
add_subdirectory(contrib)

# dummy target containing headers
add_subdirectory(pulse)

# source directory for SSD & PCM
add_subdirectory(src)
