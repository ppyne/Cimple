# CMake toolchain file for WebAssembly via Emscripten
#
# Usage:
#   emcmake cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-emscripten.cmake
#   emmake cmake --build .
#
# Or equivalently:
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-emscripten.cmake \
#            -DCMAKE_C_COMPILER=emcc

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_VERSION 1)

# Find emcc
find_program(CMAKE_C_COMPILER emcc REQUIRED)
find_program(CMAKE_AR          emar REQUIRED)
find_program(CMAKE_RANLIB      emranlib REQUIRED)

set(CMAKE_EXECUTABLE_SUFFIX ".js")

# Emscripten-specific link flags
set(CMAKE_EXE_LINKER_FLAGS
    "-s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s FORCE_FILESYSTEM=1"
    CACHE STRING "Emscripten link flags" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
