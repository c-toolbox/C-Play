# FindOMT.cmake
# Locate the OMT (Open Media Transport) library
#
# This module defines:
#   OMT_FOUND        - True if OMT was found
#   OMT_INCLUDE_DIRS - Include directories for OMT
#   OMT_LIBRARIES    - Libraries to link against
#   OMT_BINARY_DIR   - Directory containing runtime DLLs
#   OMT::OMT         - Imported target

if(NOT OMT_ROOT)
    set(OMT_ROOT $ENV{OMT_ROOT})
endif()

find_path(OMT_INCLUDE_DIR
    NAMES libomt.h
    PATHS
        ${OMT_ROOT}
        ${OMT_ROOT}/Libraries/Winx64
        ${OMT_ROOT}/libomt
)

find_library(OMT_LIBRARY
    NAMES libomt
    PATHS
        ${OMT_ROOT}
        ${OMT_ROOT}/Libraries/Winx64
        ${OMT_ROOT}/lib
)

find_path(OMT_BINARY_DIR
    NAMES libomt.dll
    PATHS
        ${OMT_ROOT}
        ${OMT_ROOT}/Libraries/Winx64
        ${OMT_ROOT}/bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OMT
    REQUIRED_VARS OMT_LIBRARY OMT_INCLUDE_DIR
)

if(OMT_FOUND)
    set(OMT_INCLUDE_DIRS ${OMT_INCLUDE_DIR})
    set(OMT_LIBRARIES ${OMT_LIBRARY})

    if(NOT TARGET OMT::OMT)
        add_library(OMT::OMT SHARED IMPORTED)
        set_target_properties(OMT::OMT PROPERTIES
            IMPORTED_CONFIGURATIONS "Release;Debug;RelWithDebInfo;MinSizeRel"
            IMPORTED_LOCATION_RELEASE "${OMT_BINARY_DIR}/libomt.dll"
            IMPORTED_LOCATION_DEBUG "${OMT_BINARY_DIR}/libomt.dll"
            IMPORTED_LOCATION_RELWITHDEBINFO "${OMT_BINARY_DIR}/libomt.dll"
            IMPORTED_LOCATION_MINSIZEREL "${OMT_BINARY_DIR}/libomt.dll"
            IMPORTED_IMPLIB_RELEASE "${OMT_LIBRARY}"
            IMPORTED_IMPLIB_DEBUG "${OMT_LIBRARY}"
            IMPORTED_IMPLIB_RELWITHDEBINFO "${OMT_LIBRARY}"
            IMPORTED_IMPLIB_MINSIZEREL "${OMT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OMT_INCLUDE_DIRS}"
        )
    endif()
endif()

mark_as_advanced(OMT_INCLUDE_DIR OMT_LIBRARY OMT_BINARY_DIR)
