# FindRGBEasy.cmake
# Locate the Datapath RGBEasy SDK (capture card SDK for Datapath Vision cards).
#
# Set RGBEASY_ROOT (CMake or environment variable) to the SDK folder that contains
# C/INCLUDE and C/LIB, e.g. D:/Datapath/RGBEASY.
#
# The runtime (RGBEasy.dll) is installed by the Datapath driver package and is not shipped
# with C-Play; C-Play delay-loads it so machines without the driver still start.
#
# This module defines:
#   RGBEasy_FOUND        - True if the SDK was found
#   RGBEasy_INCLUDE_DIRS - Include directories for the SDK
#   RGBEasy_LIBRARIES    - Import library to link against
#   RGBEasy::RGBEasy     - Imported target

if(NOT RGBEASY_ROOT)
    set(RGBEASY_ROOT $ENV{RGBEASY_ROOT})
endif()

find_path(RGBEasy_INCLUDE_DIR
    NAMES RGBAPI.H rgbapi.h
    PATHS
        ${RGBEASY_ROOT}
        ${RGBEASY_ROOT}/C/INCLUDE
        ${RGBEASY_ROOT}/INCLUDE
        ${RGBEASY_ROOT}/include
    NO_DEFAULT_PATH
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_rgbeasy_arch x64)
else()
    set(_rgbeasy_arch Win32)
endif()

find_library(RGBEasy_LIBRARY
    NAMES RGBEASY RGBEasy rgbeasy
    PATHS
        ${RGBEASY_ROOT}/C/LIB/${_rgbeasy_arch}/Release
        ${RGBEASY_ROOT}/LIB/${_rgbeasy_arch}/Release
        ${RGBEASY_ROOT}/lib/${_rgbeasy_arch}
        ${RGBEASY_ROOT}/lib
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RGBEasy
    REQUIRED_VARS RGBEasy_LIBRARY RGBEasy_INCLUDE_DIR
)

if(RGBEasy_FOUND)
    set(RGBEasy_INCLUDE_DIRS ${RGBEasy_INCLUDE_DIR})
    set(RGBEasy_LIBRARIES ${RGBEasy_LIBRARY})

    if(NOT TARGET RGBEasy::RGBEasy)
        add_library(RGBEasy::RGBEasy UNKNOWN IMPORTED)
        # Plain C import library: the release build is used for every configuration.
        set_target_properties(RGBEasy::RGBEasy PROPERTIES
            IMPORTED_CONFIGURATIONS "Release;Debug;RelWithDebInfo;MinSizeRel"
            IMPORTED_LOCATION "${RGBEasy_LIBRARY}"
            IMPORTED_LOCATION_RELEASE "${RGBEasy_LIBRARY}"
            IMPORTED_LOCATION_DEBUG "${RGBEasy_LIBRARY}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${RGBEasy_LIBRARY}"
            IMPORTED_LOCATION_MINSIZEREL "${RGBEasy_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${RGBEasy_INCLUDE_DIRS}"
        )
    endif()
endif()

mark_as_advanced(RGBEasy_INCLUDE_DIR RGBEasy_LIBRARY)
