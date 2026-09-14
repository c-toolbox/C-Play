# Locates the pinned FFmpeg build tree that the mpv client library was built against.
#
# The WebRTC layer decodes with the same FFmpeg runtime that mpv already ships, so
# no extra DLLs are introduced and there is no risk of two FFmpeg versions being
# loaded side by side. By default the root is derived from Libmpv_LIBRARIES (the
# import lib lives in <root>/bin-video next to the av*.lib import libs). Override
# with CPLAY_FFMPEG_ROOT if a different tree should be used.
#
# Components: AVCODEC AVUTIL SWSCALE
#
# Imported targets:
#   FFmpegPinned::<COMPONENT>  one per found component
#   FFmpegPinned::FFmpeg       aggregate of all requested components
#
# Result variables:
#   FFmpegPinned_FOUND, FFmpegPinned_INCLUDE_DIRS, FFmpegPinned_LIBRARIES,
#   FFmpegPinned_RUNTIME_DIR (directory holding the runtime DLLs),
#   FFmpegPinned_VERSION_STRING

include(FindPackageHandleStandardArgs)

set(CPLAY_FFMPEG_LIBDIR_NAME "bin-video"
    CACHE STRING "Directory under CPLAY_FFMPEG_ROOT holding the import libs and DLLs")

if(NOT CPLAY_FFMPEG_ROOT AND Libmpv_LIBRARIES)
    get_filename_component(_cplay_ffmpeg_libdir "${Libmpv_LIBRARIES}" DIRECTORY)
    # <root>/bin-video/mpv.lib -> <root>
    get_filename_component(CPLAY_FFMPEG_ROOT_DEFAULT "${_cplay_ffmpeg_libdir}" DIRECTORY)
else()
    set(CPLAY_FFMPEG_ROOT_DEFAULT "")
endif()

set(CPLAY_FFMPEG_ROOT "${CPLAY_FFMPEG_ROOT_DEFAULT}"
    CACHE PATH "Root of the FFmpeg build tree (contains include/ and ${CPLAY_FFMPEG_LIBDIR_NAME}/)")

if(NOT CPLAY_FFMPEG_ROOT)
    message(FATAL_ERROR
        "FFmpegPinned: could not determine the FFmpeg root. Set CPLAY_FFMPEG_ROOT to a prefix "
        "containing include/ and ${CPLAY_FFMPEG_LIBDIR_NAME}/ (it is normally derived from Libmpv).")
endif()

file(TO_CMAKE_PATH "${CPLAY_FFMPEG_ROOT}" _ffmpeg_root)
set(_ffmpeg_libdir "${_ffmpeg_root}/${CPLAY_FFMPEG_LIBDIR_NAME}")

find_path(FFmpegPinned_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    PATHS "${_ffmpeg_root}/include"
    NO_DEFAULT_PATH
)

# Read the version out of the headers; there is no pkg-config on Windows here.
set(FFmpegPinned_VERSION_STRING "unknown")
if(FFmpegPinned_INCLUDE_DIR AND EXISTS "${FFmpegPinned_INCLUDE_DIR}/libavutil/ffversion.h")
    file(STRINGS "${FFmpegPinned_INCLUDE_DIR}/libavutil/ffversion.h" _ffversion_line
         REGEX "^#define[ \t]+FFMPEG_VERSION[ \t]+\"")
    if(_ffversion_line)
        string(REGEX REPLACE "^#define[ \t]+FFMPEG_VERSION[ \t]+\"([^\"]+)\".*$" "\\1"
               FFmpegPinned_VERSION_STRING "${_ffversion_line}")
    endif()
endif()

if(NOT FFmpegPinned_FIND_COMPONENTS)
    set(FFmpegPinned_FIND_COMPONENTS AVCODEC AVUTIL SWSCALE)
endif()

set(_ffmpeg_all_components AVCODEC AVUTIL SWSCALE)

foreach(_comp IN LISTS _ffmpeg_all_components)
    string(TOLOWER "${_comp}" _lib)

    find_library(${_comp}_LIBRARY
        NAMES ${_lib}
        PATHS "${_ffmpeg_libdir}" "${_ffmpeg_root}/lib"
        NO_DEFAULT_PATH
    )
    mark_as_advanced(${_comp}_LIBRARY)

    if(${_comp}_LIBRARY AND FFmpegPinned_INCLUDE_DIR)
        set(FFmpegPinned_${_comp}_FOUND TRUE)
        set(${_comp}_FOUND TRUE)

        if(NOT TARGET FFmpegPinned::${_comp})
            add_library(FFmpegPinned::${_comp} UNKNOWN IMPORTED)
            # The top level maps imported target configurations (CMAKE_MAP_IMPORTED_CONFIG_*),
            # so set every per-configuration location explicitly, like FindLibmpv does.
            set_target_properties(FFmpegPinned::${_comp} PROPERTIES
                IMPORTED_LOCATION "${${_comp}_LIBRARY}"
                IMPORTED_LOCATION_DEBUG "${${_comp}_LIBRARY}"
                IMPORTED_LOCATION_RELEASE "${${_comp}_LIBRARY}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${${_comp}_LIBRARY}"
                IMPORTED_LOCATION_MINSIZEREL "${${_comp}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FFmpegPinned_INCLUDE_DIR}"
            )
        endif()
    else()
        set(FFmpegPinned_${_comp}_FOUND FALSE)
        set(${_comp}_FOUND FALSE)
    endif()
endforeach()

set(FFmpegPinned_LIBRARIES "")
foreach(_comp IN LISTS FFmpegPinned_FIND_COMPONENTS)
    if(${_comp}_FOUND)
        list(APPEND FFmpegPinned_LIBRARIES "${${_comp}_LIBRARY}")
    endif()
endforeach()

set(FFmpegPinned_INCLUDE_DIRS "${FFmpegPinned_INCLUDE_DIR}")
if(EXISTS "${_ffmpeg_libdir}")
    set(FFmpegPinned_RUNTIME_DIR "${_ffmpeg_libdir}")
endif()

find_package_handle_standard_args(FFmpegPinned
    REQUIRED_VARS FFmpegPinned_INCLUDE_DIR FFmpegPinned_LIBRARIES
    VERSION_VAR FFmpegPinned_VERSION_STRING
    HANDLE_COMPONENTS
)

if(FFmpegPinned_FOUND AND NOT TARGET FFmpegPinned::FFmpeg)
    add_library(FFmpegPinned::FFmpeg INTERFACE IMPORTED)
    foreach(_comp IN LISTS FFmpegPinned_FIND_COMPONENTS)
        if(${_comp}_FOUND)
            set_property(TARGET FFmpegPinned::FFmpeg APPEND
                         PROPERTY INTERFACE_LINK_LIBRARIES FFmpegPinned::${_comp})
        endif()
    endforeach()
endif()

mark_as_advanced(FFmpegPinned_INCLUDE_DIR)
