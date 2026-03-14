cmake_minimum_required(VERSION 3.15)
project(cimple C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# --- Options ---
option(CIMPLE_ENABLE_TESTS "Build tests" ON)

# --- Directories ---
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(TOOLS_DIR "${CMAKE_SOURCE_DIR}/tools")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

# --- Find re2c ---
find_program(RE2C_EXECUTABLE re2c REQUIRED)
message(STATUS "Found re2c: ${RE2C_EXECUTABLE}")

# --- Find lemon (build from tools/ if not in PATH) ---
find_program(LEMON_EXECUTABLE lemon)
if(NOT LEMON_EXECUTABLE)
    message(STATUS "lemon not found in PATH, building from tools/")
    add_executable(lemon_tool "${TOOLS_DIR}/lemon.c")
    target_compile_options(lemon_tool PRIVATE -w)
    if(EMSCRIPTEN)
        target_link_options(lemon_tool PRIVATE "-sNODERAWFS=1")
    endif()
    set(LEMON_EXECUTABLE $<TARGET_FILE:lemon_tool>)
    set(LEMON_BUILT_HERE TRUE)
endif()

set(LEMON_COMMAND "${LEMON_EXECUTABLE}")
if(LEMON_BUILT_HERE AND CMAKE_CROSSCOMPILING_EMULATOR)
    set(LEMON_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} "${LEMON_EXECUTABLE}")
endif()

# --- Generate lexer with re2c ---
set(LEXER_RE "${SRC_DIR}/lexer/lexer.re")
set(LEXER_C  "${GENERATED_DIR}/lexer.c")
add_custom_command(
    OUTPUT  "${LEXER_C}"
    COMMAND "${RE2C_EXECUTABLE}" -W -o "${LEXER_C}" "${LEXER_RE}"
    DEPENDS "${LEXER_RE}"
    COMMENT "Generating lexer from ${LEXER_RE}"
    VERBATIM
)

# --- Generate parser with Lemon ---
set(PARSER_Y  "${SRC_DIR}/parser/parser.y")
set(PARSER_C  "${GENERATED_DIR}/parser.c")
set(PARSER_H  "${GENERATED_DIR}/parser.h")
add_custom_command(
    OUTPUT  "${PARSER_C}" "${PARSER_H}"
    COMMAND ${LEMON_COMMAND} "-d${GENERATED_DIR}" "${PARSER_Y}"
    DEPENDS "${PARSER_Y}"
            $<$<BOOL:${LEMON_BUILT_HERE}>:lemon_tool>
    COMMENT "Generating parser from ${PARSER_Y}"
    WORKING_DIRECTORY "${GENERATED_DIR}"
    VERBATIM
)
# Copy lempar.c template so lemon can find it at generation time
configure_file("${TOOLS_DIR}/lempar.c" "${GENERATED_DIR}/lempar.c" COPYONLY)

# --- Cimple sources ---
set(CIMPLE_SOURCES
    "${SRC_DIR}/common/error.c"
    "${SRC_DIR}/common/memory.c"
    "${SRC_DIR}/ast/ast.c"
    "${SRC_DIR}/runtime/value.c"
    "${SRC_DIR}/runtime/scope.c"
    "${SRC_DIR}/runtime/regex_engine.c"
    "${SRC_DIR}/semantic/semantic.c"
    "${SRC_DIR}/interpreter/interpreter.c"
    "${SRC_DIR}/interpreter/builtins.c"
    "${SRC_DIR}/parser/parser_helper.c"
    "${LEXER_C}"
    "${PARSER_C}"
    "${SRC_DIR}/main.c"
    "${TOOLS_DIR}/utf8proc.c"
)

# --- Executable ---
add_executable(cimple ${CIMPLE_SOURCES})
target_include_directories(cimple PRIVATE
    "${SRC_DIR}"
    "${SRC_DIR}/lexer"
    "${SRC_DIR}/parser"
    "${GENERATED_DIR}"
    "${TOOLS_DIR}"
)

# --- Portable compiler flags ---
if(MSVC)
    target_compile_options(cimple PRIVATE /W3 /wd4996)
else()
    target_compile_options(cimple PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable)
endif()

# --- Math library (POSIX) ---
if(UNIX)
    target_link_libraries(cimple m)
endif()

# --- Tests ---
if(CIMPLE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Install ---
install(TARGETS cimple RUNTIME DESTINATION bin)

message(STATUS "Cimple build configured:")
message(STATUS "  Source dir:    ${SRC_DIR}")
message(STATUS "  Generated dir: ${GENERATED_DIR}")
message(STATUS "  Build type:    ${CMAKE_BUILD_TYPE}")
