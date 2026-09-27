cmake_minimum_required(VERSION 3.14)

project(ORG4C
    VERSION "0.1.0"
    HOMEPAGE_URL "https://github.com/unindented/org4c"
    LANGUAGES C
)

# Some heuristics for what targets (and how) we actually build:
set(BUILD_SHARED_LIBS_default ON)
set(BUILD_HTML_LIBRARY_default ON)
set(BUILD_ORG2HTML_EXECUTABLE_default ON)
if(WIN32)
    # On Windows, given there is no standard lib install dir etc., we
    # rather by default build static lib.
    set(BUILD_SHARED_LIBS_default OFF)
endif()
if(NOT PROJECT_IS_TOP_LEVEL)
    # When we're embedded into a bigger project, most likely the outer
    # project only needs the libs.
    set(BUILD_ORG2HTML_EXECUTABLE_default OFF)
endif()

# Allow the user to override the defaults:
option(
    BUILD_SHARED_LIBS
    "Build using shared libraries"
    ${BUILD_SHARED_LIBS_default}
)
option(
    BUILD_HTML_LIBRARY
    "Whether to compile the liborg4c-html library"
    ${BUILD_HTML_LIBRARY_default}
)
option(
    BUILD_ORG2HTML_EXECUTABLE
    "Whether to compile the org2html command line tool (requires liborg4c-html)"
    ${BUILD_ORG2HTML_EXECUTABLE_default}
)

if(BUILD_ORG2HTML_EXECUTABLE AND NOT BUILD_HTML_LIBRARY)
    message(FATAL_ERROR "BUILD_ORG2HTML_EXECUTABLE requires BUILD_HTML_LIBRARY=ON")
endif()


set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE $ENV{CMAKE_BUILD_TYPE})

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
endif()

if(CMAKE_C_COMPILER_ID MATCHES GNU|Clang)
    add_compile_options(-Wall -Wextra -Wshadow)

    # We enforce -Wdeclaration-after-statement (as MD4C does) so the code
    # builds also with compilers which choke whenever a declaration is not at
    # the beginning of a block.
    add_compile_options(-Wdeclaration-after-statement)
elseif(MSVC)
    # Disable warnings about the so-called unsecured functions:
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3)

    # Specify proper C runtime library:
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()

include(GNUInstallDirs)

add_subdirectory(src)
add_subdirectory(org2html)
