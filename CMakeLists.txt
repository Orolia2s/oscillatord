cmake_minimum_required(VERSION 2.8)
find_package(PkgConfig)

set(PROJECT_NAME "oscillatord")

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/version PACKAGE_VERSION)
message("${PROJECT_NAME} version is ${PACKAGE_VERSION}")
STRING(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" VERSION_MAJOR "${PACKAGE_VERSION}")
STRING(REGEX REPLACE "^[0-9]+\\.([0-9])+\\.[0-9]+" "\\1" VERSION_MINOR "${PACKAGE_VERSION}")
STRING(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" VERSION_PATCH "${PACKAGE_VERSION}")
set(PACKAGE_NAME "lib${PROJECT_NAME}")
set(PACKAGE_DESCRIPTION_SUMMARY "Oscillator disciplining daemon.")

project(${PROJECT_NAME})
include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)

pkg_check_modules(oscillator-disciplining REQUIRED liboscillator-disciplining)
pkg_check_modules(spi2c REQUIRED libspi2c)
pkg_check_modules(tsync REQUIRED libtsync)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)
include_directories(${oscillator-disciplining_INCLUDE_DIRS})
include_directories(${spi2c_INCLUDE_DIRS})
include_directories(${tsync_INCLUDE_DIRS})

add_definitions("
    -O3
    -Wall
    -Wextra
    -Wformat=2
    -Wunused-variable
    -Wold-style-definition
    -Wstrict-prototypes
    -Wno-unused-parameter
    -Wmissing-declarations
    -Wmissing-prototypes
    -Wpointer-arith")
add_definitions("-DOD_REVISION=\"${PACKAGE_VERSION}\"")

add_subdirectory(tests)
add_subdirectory(src)
