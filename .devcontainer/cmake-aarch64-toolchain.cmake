# CMake toolchain file for aarch64 cross-compilation.
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=.devcontainer/cmake-aarch64-toolchain.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "-march=armv8-a" CACHE STRING "C Flags")
set(CMAKE_CXX_FLAGS "-march=armv8-a" CACHE STRING "CXX Flags")
