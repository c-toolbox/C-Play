# FindDeckLink.cmake
# Locate the Blackmagic DeckLink SDK (capture card SDK for Blackmagic DeckLink/UltraStudio/Intensity cards).
#
# Set DECKLINK_SDK_ROOT (CMake or environment variable) to the SDK folder that contains
# Win/include/DeckLinkAPI.idl, e.g. "D:/Blackmagic/Blackmagic DeckLink SDK 16.0".
#
# The Windows SDK only ships IDL files: this module runs MIDL at configure time to generate
# DeckLinkAPI_h.h (declarations) and DeckLinkAPI_i.c (interface and class IDs, included by the
# one source file that uses them). There is no import library: the runtime (DeckLinkAPI64.dll)
# is a COM server registered by the Blackmagic Desktop Video driver package and is not shipped
# with C-Play, so machines without the driver still start.
#
# This module defines:
#   DeckLink_FOUND        - True if the SDK was found and the headers were generated
#   DeckLink_INCLUDE_DIRS - Include directory of the generated headers
#   DeckLink::DeckLink    - Imported interface target

if(NOT DECKLINK_SDK_ROOT)
    set(DECKLINK_SDK_ROOT $ENV{DECKLINK_SDK_ROOT})
endif()

find_path(DeckLink_IDL_DIR
    NAMES DeckLinkAPI.idl
    PATHS
        ${DECKLINK_SDK_ROOT}/Win/include
        ${DECKLINK_SDK_ROOT}/include
        ${DECKLINK_SDK_ROOT}
    NO_DEFAULT_PATH
)

# MIDL ships with the Windows SDK and uses cl.exe as its preprocessor.
set(_decklink_sdk_version "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
if(NOT _decklink_sdk_version)
    set(_decklink_sdk_version "$ENV{WindowsSDKVersion}")
    string(REGEX REPLACE "[\\/]$" "" _decklink_sdk_version "${_decklink_sdk_version}")
endif()
get_filename_component(_decklink_kits_root
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_decklink_arch x64)
else()
    set(_decklink_arch x86)
endif()

find_program(DeckLink_MIDL_EXECUTABLE
    NAMES midl
    HINTS
        "${_decklink_kits_root}/bin/${_decklink_sdk_version}/${_decklink_arch}"
        "$ENV{WindowsSdkVerBinPath}/${_decklink_arch}"
)

set(_decklink_out_dir "${CMAKE_BINARY_DIR}/decklink")
set(_decklink_header "${_decklink_out_dir}/DeckLinkAPI_h.h")
set(_decklink_iid "${_decklink_out_dir}/DeckLinkAPI_i.c")

if(DeckLink_IDL_DIR AND DeckLink_MIDL_EXECUTABLE AND CMAKE_CXX_COMPILER)
    set(_decklink_idl "${DeckLink_IDL_DIR}/DeckLinkAPI.idl")
    file(GLOB _decklink_idls "${DeckLink_IDL_DIR}/*.idl")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_decklink_idls})

    set(_decklink_regenerate TRUE)
    if(EXISTS "${_decklink_header}" AND EXISTS "${_decklink_iid}"
       AND NOT "${_decklink_idl}" IS_NEWER_THAN "${_decklink_header}"
       AND "${DeckLink_GENERATED_FROM}" STREQUAL "${_decklink_idl}")
        set(_decklink_regenerate FALSE)
    endif()

    if(_decklink_regenerate)
        file(MAKE_DIRECTORY "${_decklink_out_dir}")
        set(_decklink_includes "")
        if(_decklink_sdk_version AND EXISTS "${_decklink_kits_root}/Include/${_decklink_sdk_version}/um")
            list(APPEND _decklink_includes
                /I "${_decklink_kits_root}/Include/${_decklink_sdk_version}/um"
                /I "${_decklink_kits_root}/Include/${_decklink_sdk_version}/shared")
        endif()
        # midl cannot take a quoted /cpp_cmd path with spaces: put cl.exe on PATH instead.
        get_filename_component(_decklink_cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(TO_NATIVE_PATH "${_decklink_cl_dir}" _decklink_cl_dir_native)
        set(_decklink_path "${_decklink_cl_dir_native};$ENV{PATH}")
        string(REPLACE ";" "\\;" _decklink_path "${_decklink_path}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${_decklink_path}"
                    "${DeckLink_MIDL_EXECUTABLE}" /nologo /env ${_decklink_arch} /W1 /char signed
                    ${_decklink_includes} /I "${DeckLink_IDL_DIR}"
                    /h DeckLinkAPI_h.h /iid DeckLinkAPI_i.c /tlb DeckLinkAPI.tlb
                    /out "${_decklink_out_dir}" "${_decklink_idl}"
            RESULT_VARIABLE _decklink_midl_result
            OUTPUT_VARIABLE _decklink_midl_output
            ERROR_VARIABLE _decklink_midl_output
        )
        if(NOT _decklink_midl_result EQUAL 0)
            message(WARNING "DeckLink: MIDL failed on ${_decklink_idl} (${_decklink_midl_result}):\n${_decklink_midl_output}")
            file(REMOVE "${_decklink_header}" "${_decklink_iid}")
        else()
            set(DeckLink_GENERATED_FROM "${_decklink_idl}" CACHE INTERNAL "IDL the DeckLink headers were generated from")
        endif()
    endif()
endif()

if(EXISTS "${_decklink_header}" AND EXISTS "${_decklink_iid}")
    set(DeckLink_INCLUDE_DIR "${_decklink_out_dir}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DeckLink
    REQUIRED_VARS DeckLink_INCLUDE_DIR DeckLink_IDL_DIR DeckLink_MIDL_EXECUTABLE
)

if(DeckLink_FOUND)
    set(DeckLink_INCLUDE_DIRS ${DeckLink_INCLUDE_DIR})

    if(NOT TARGET DeckLink::DeckLink)
        add_library(DeckLink::DeckLink INTERFACE IMPORTED)
        set_target_properties(DeckLink::DeckLink PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${DeckLink_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "ole32;oleaut32"
        )
    endif()
endif()

# NVIDIA GPUDirect for Video (DVP), shipped with the SDK's OpenGL samples: DMA of captured frames
# straight into GPU memory on NVIDIA professional GPUs. Optional; dvp.dll is redistributable and
# is delay-loaded, so it is only needed on machines that use it.
#   DeckLink_DVP_FOUND  - True if DVPAPI.h, dvp.lib and dvp.dll were found
#   DeckLink_DVP_DLL    - dvp.dll to ship next to the executable
#   DeckLink::DVP       - Imported interface target (include dir is for the one source using it:
#                         the folder also holds GL/glew/pthread headers that must not shadow others)
if(DeckLink_FOUND)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_decklink_dvp_arch x64)
    else()
        set(_decklink_dvp_arch Win32)
    endif()
    find_path(DeckLink_DVP_INCLUDE_DIR
        NAMES DVPAPI.h dvpapi_gl.h
        PATHS ${DECKLINK_SDK_ROOT}/Win/Samples/NVIDIA_GPUDirect/include
        NO_DEFAULT_PATH
    )
    find_library(DeckLink_DVP_LIBRARY
        NAMES dvp
        PATHS ${DECKLINK_SDK_ROOT}/Win/Samples/NVIDIA_GPUDirect/lib/${_decklink_dvp_arch}
        NO_DEFAULT_PATH
    )
    string(TOLOWER "${_decklink_dvp_arch}" _decklink_dvp_bin_arch)
    find_file(DeckLink_DVP_DLL
        NAMES dvp.dll
        PATHS ${DECKLINK_SDK_ROOT}/Win/Samples/NVIDIA_GPUDirect/bin/${_decklink_dvp_bin_arch}
        NO_DEFAULT_PATH
    )
    if(DeckLink_DVP_INCLUDE_DIR AND DeckLink_DVP_LIBRARY AND DeckLink_DVP_DLL)
        set(DeckLink_DVP_FOUND TRUE)
        if(NOT TARGET DeckLink::DVP)
            add_library(DeckLink::DVP INTERFACE IMPORTED)
            set_target_properties(DeckLink::DVP PROPERTIES
                INTERFACE_LINK_LIBRARIES "${DeckLink_DVP_LIBRARY};delayimp"
                INTERFACE_LINK_OPTIONS "/DELAYLOAD:dvp.dll"
            )
        endif()
    else()
        set(DeckLink_DVP_FOUND FALSE)
    endif()
endif()

mark_as_advanced(DeckLink_IDL_DIR DeckLink_MIDL_EXECUTABLE DeckLink_DVP_INCLUDE_DIR DeckLink_DVP_LIBRARY DeckLink_DVP_DLL)
