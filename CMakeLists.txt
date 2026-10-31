# 3.20 is the floor: it is what Ubuntu 22.04 ships and what the CMake versions
# bundled with recent Visual Studio and Homebrew provide. CMake 4.x refuses
# projects that ask for a policy version below 3.5, so this also keeps the build
# working on the newest toolchains.
cmake_minimum_required(VERSION 3.20)

project(newisp
    VERSION 1.0.0
    DESCRIPTION "STC MCU programmer (serial + USB HID), cross-platform CLI"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
#  Language level
# ---------------------------------------------------------------------------
# C++17 is the floor: the code uses std::string_view-free constructs but relies
# on inline variables, std::make_unique and if-init. Nothing here needs newer.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# ---------------------------------------------------------------------------
#  Source encoding
# ---------------------------------------------------------------------------
# The log messages in this project are Chinese, and the source files are UTF-8.
# MSVC defaults to the system ANSI code page (936/GBK on a Chinese Windows),
# which mangles those string literals into "newline in constant" errors.
#
# Two things are done about it, because either alone is fragile:
#
#   * /utf-8 tells MSVC the source and the execution character set are both
#     UTF-8. This is the authoritative fix.
#   * Every file is saved with a UTF-8 BOM, which is what Visual Studio itself
#     writes and what GCC/Clang also accept. It keeps the files unambiguous
#     even when compiled by a tool that ignores the flag.

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
    # /W4 is noisy about the C++ standard library on some MSVC versions; the
    # ported protocol code is C++17-clean, so keep the external headers quiet.
    add_compile_options(/external:W0)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
#  Options
# ---------------------------------------------------------------------------
# USB HID support is required, not optional. It is the only transport the 8051U
# family has, so a build without it would be a build that cannot program part of
# the catalogue -- and it would fail silently, at run time, on somebody's bench.
#
#   Windows      the backend talks to the system HID class driver through
#                hid.dll and SetupAPI, both of which ship with the operating
#                system, so nothing external is needed.
#   macOS/Linux  hidapi, which wraps IOKit and hidraw. It is a hard dependency
#                there: configure stops with an explanation instead of
#                producing a partial tool.

option(NEWISP_BUILD_TESTS "Build the unit tests" OFF)

# The language this build speaks. Only that table is compiled in; the other
# language is not in the binary. English is the source language of the message
# table and Chinese is the project's primary audience, so both are first class:
#
#   cmake -DNEWISP_LANG=zh-CN   Chinese (default)
#   cmake -DNEWISP_LANG=en      English
#
# To ship both, configure twice into two build directories.
set(NEWISP_LANG "zh-CN" CACHE STRING "Output language: zh-CN or en")
set_property(CACHE NEWISP_LANG PROPERTY STRINGS "zh-CN" "en")

string(TOLOWER "${NEWISP_LANG}" NEWISP_LANG_LC)
if(NEWISP_LANG_LC STREQUAL "zh-cn" OR NEWISP_LANG_LC STREQUAL "zh")
    set(NEWISP_LANG_CODE_DEFINE NEWISP_LANG_CODE_CHINESE=1)
elseif(NEWISP_LANG_LC STREQUAL "en" OR NEWISP_LANG_LC MATCHES "^en[-_]")
    set(NEWISP_LANG_CODE_DEFINE NEWISP_LANG_CODE_ENGLISH=1)
else()
    message(FATAL_ERROR
        "NEWISP_LANG must be zh-CN or en, got '${NEWISP_LANG}'")
endif()
message(STATUS "Output language: ${NEWISP_LANG}")

if(NOT WIN32)
    find_package(hidapi QUIET)
    if(NOT hidapi_FOUND)
        # pkg-config fallback: most distributions ship hidapi this way and the
        # CMake config package is comparatively recent.
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(HIDAPI QUIET hidapi-hidraw)
            if(NOT HIDAPI_FOUND)
                pkg_check_modules(HIDAPI QUIET hidapi)
            endif()
        endif()
    endif()

    if(NOT hidapi_FOUND AND NOT HIDAPI_FOUND)
        message(FATAL_ERROR
            "hidapi is required: USB HID is the only transport for the 8051U "
            "family. Install libhidapi-dev (Debian/Ubuntu), hidapi-devel "
            "(Fedora) or 'brew install hidapi' (macOS).")
    endif()
endif()

# ---------------------------------------------------------------------------
#  Core library: chip tables, Intel HEX parsing, ISP protocols
# ---------------------------------------------------------------------------
# This target is pure C++ with no platform dependencies at all, which is what
# lets the same protocol code build on all three platforms.

add_library(newisp_core STATIC
    src/core/chip_table.cpp
    src/core/chip_logic.cpp
    src/core/hex_file.cpp
    src/core/i18n.cpp
)

target_include_directories(newisp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# The message tables live next to the translations they belong to, not in the
# public header directory: text_en.inc is the reference table and src/i18n/
# holds one file per language. i18n.cpp includes them by relative path, so only
# the source directory has to be visible here.
set_property(SOURCE src/core/i18n.cpp APPEND PROPERTY
    OBJECT_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/newisp/text_en.inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/i18n/text_zh_cn.inc
)

target_compile_features(newisp_core PUBLIC cxx_std_17)

# Only the selected language's table is compiled. This is a PRIVATE definition
# because nothing outside i18n.cpp needs to know; public code asks Tr() for a
# string instead of branching on the language.
target_compile_definitions(newisp_core PRIVATE ${NEWISP_LANG_CODE_DEFINE})

# The protocol layer is header-only and pulls in <thread>; nothing else is
# needed on any of the three platforms.
find_package(Threads REQUIRED)
target_link_libraries(newisp_core PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
#  Transport library: serial port + USB HID device access
# ---------------------------------------------------------------------------
# Everything platform-specific is confined to this target.

add_library(newisp_transport STATIC
    src/transport/serial_channel.cpp
    src/transport/serial_enum.cpp
    src/transport/hid_channel.cpp
)

if(WIN32)
    target_sources(newisp_transport PRIVATE src/transport/serial_channel_win32.cpp)
else()
    target_sources(newisp_transport PRIVATE src/transport/serial_channel_posix.cpp)
endif()

target_link_libraries(newisp_transport PUBLIC newisp_core)

# USB HID is part of the product, not a switch; see the Options section. The
# definition is PUBLIC because newisp/hid_channel.h checks it.
target_compile_definitions(newisp_transport PUBLIC NEWISP_HAVE_HID=1)

# serial_channel_impl.h is a private header shared by the three transport
# translation units, so the directory has to be on the include path.
target_include_directories(newisp_transport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transport)

if(WIN32)
    # The Windows HID backend links against the system HID class driver.
    # hid.lib carries both the HidD_* class accessors and the HidP_* report
    # parser (HidP_GetCaps); setupapi covers device enumeration. Neither needs
    # to be shipped, so the Windows build has no external dependency.
    target_link_libraries(newisp_transport PRIVATE setupapi hid)
elseif(APPLE)
    # IOKit provides the serial device list on macOS.
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(newisp_transport PRIVATE
        ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
endif()

if(NOT WIN32)
    # Required on these platforms; the configure step already failed if it is
    # missing.
    if(hidapi_FOUND)
        # Target name differs between the CMake package and older releases.
        if(TARGET hidapi::hidapi)
            target_link_libraries(newisp_transport PUBLIC hidapi::hidapi)
        elseif(TARGET hidapi::hidapi-hidraw)
            target_link_libraries(newisp_transport PUBLIC hidapi::hidapi-hidraw)
        elseif(TARGET hidapi::hidapi-libusb)
            target_link_libraries(newisp_transport PUBLIC hidapi::hidapi-libusb)
        else()
            target_link_libraries(newisp_transport PUBLIC hidapi)
        endif()
    else()
        target_include_directories(newisp_transport PRIVATE ${HIDAPI_INCLUDE_DIRS})
        target_link_directories(newisp_transport PRIVATE ${HIDAPI_LIBRARY_DIRS})
        target_link_libraries(newisp_transport PUBLIC ${HIDAPI_LIBRARIES})
    endif()
endif()

# ---------------------------------------------------------------------------
#  CLI executable
# ---------------------------------------------------------------------------

add_executable(newisp
    src/cli/main.cpp
    src/cli/options.cpp
    src/cli/session.cpp
)

target_link_libraries(newisp PRIVATE newisp_transport)

# The version string is quoted here because the compile definition ends up in a
# command line: without the escaped quotes MSVC would see NEWISP_VERSION=1.0.0
# and reject it as an invalid numeric literal.
#
# It is defined on newisp_core rather than on the executable because i18n.cpp
# is where it is wrapped into a plain const char* (i18n::BuildVersion) for
# everyone else to use.
target_compile_definitions(newisp_core PRIVATE
    NEWISP_VERSION="${PROJECT_VERSION}")

if(WIN32)
    # Console subsystem: this is a command-line tool, so no WinMain.
    target_link_libraries(newisp PRIVATE setupapi hid)
endif()

install(TARGETS newisp RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
#  Tests
# ---------------------------------------------------------------------------

if(NEWISP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
