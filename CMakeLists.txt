cmake_minimum_required(VERSION 3.10)
project(oatmux VERSION 1.0.0 LANGUAGES C)

# Require Linux
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This project only supports Linux")
endif()

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type (default to Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Detect architecture
message(STATUS "Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")

# Common compiler flags
set(COMMON_FLAGS "-Wall -Wextra -Wpedantic -pipe")

# Architecture-specific optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    message(STATUS "Configuring for ARM64")
    set(ARCH_FLAGS "-march=armv8-a")
    if(NOT CMAKE_CROSSCOMPILING)
        set(ARCH_FLAGS "${ARCH_FLAGS} -mcpu=native")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    message(STATUS "Configuring for x86_64")
    set(ARCH_FLAGS "-march=x86-64-v2")
    if(NOT CMAKE_CROSSCOMPILING)
        option(USE_NATIVE_ARCH "Use -march=native for maximum optimization" OFF)
        if(USE_NATIVE_ARCH)
            set(ARCH_FLAGS "-march=native")
        endif()
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|armhf")
    message(STATUS "Configuring for ARMv7")
    set(ARCH_FLAGS "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
else()
    message(STATUS "Unknown architecture, using generic flags")
    set(ARCH_FLAGS "")
endif()

# Build type specific flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG -fsanitize=address,undefined")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG ${ARCH_FLAGS} -flto -fomit-frame-pointer")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG ${ARCH_FLAGS}")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG ${ARCH_FLAGS} -flto")

# Link-time optimization for Release
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} -flto")

# Debug sanitizer link flags
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Check for required headers
include(CheckIncludeFile)
check_include_file("pty.h" HAVE_PTY_H)
if(NOT HAVE_PTY_H)
    message(FATAL_ERROR "pty.h not found - required for terminal handling")
endif()

# Source files
set(SOURCES
    src/main.c
    src/server.c
    src/websocket.c
    src/terminal.c
)

# Header files (for IDEs)
set(HEADERS
    include/server.h
    include/websocket.h
    include/terminal.h
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenSSL::Crypto
    Threads::Threads
    util  # For forkpty on Linux
)

# Installation
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Configuration Summary ===")
message(STATUS "Project:        ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Architecture:   ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "C Compiler:     ${CMAKE_C_COMPILER}")
message(STATUS "C Flags:        ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "=============================")
message(STATUS "")
