cmake_minimum_required(VERSION 3.20)
find_package(PkgConfig)

set(PROJECT_NAME "oscillatord")

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/version PACKAGE_VERSION)
message("${PROJECT_NAME} version is ${PACKAGE_VERSION}")
STRING(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" VERSION_MAJOR "${PACKAGE_VERSION}")
STRING(REGEX REPLACE "^[0-9]+\\.([0-9])+\\.[0-9]+" "\\1" VERSION_MINOR "${PACKAGE_VERSION}")
STRING(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" VERSION_PATCH "${PACKAGE_VERSION}")
set(PACKAGE_NAME "lib${PROJECT_NAME}")
set(PACKAGE_DESCRIPTION_SUMMARY "Oscillator disciplining daemon.")
set(CMAKE_C_STANDARD 11)

project(${PROJECT_NAME} C)
include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

pkg_check_modules(oscillator-disciplining REQUIRED liboscillator-disciplining)
pkg_check_modules(ubloxcfg REQUIRED libubloxcfg)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/)
include_directories(${oscillator-disciplining_INCLUDE_DIRS})
include_directories(${gps_INCLUDE_DIRS})
include_directories(${ubloxcfg_INCLUDE_DIRS})

add_definitions("
    -O0
    -Wall
    -Wextra
    -Wformat=2
    -Wunused-variable
    -Wunused-function
    -Wold-style-definition
    -Wstrict-prototypes
    -Wno-unused-parameter
    -Wmissing-declarations
    -Wmissing-prototypes
    -Wpointer-arith
    -DLOG_USE_COLOR")
add_definitions("-DOD_REVISION=\"${PACKAGE_VERSION}\"")

add_subdirectory(tests)
add_subdirectory(src)
add_subdirectory(systemd)
