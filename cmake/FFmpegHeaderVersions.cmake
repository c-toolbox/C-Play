#
# Copyright: 2026 Erik Sunden <eriksunden85@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Varoius packages ship FFmpeg headers too (e.g. Craft), so an include order change 
# could silently compile a target against headers of another FFmpeg than the one it links.
#
# cplay_ffmpeg_header_majors(<include_dir> <out_var> <lib>...)
#   Reads LIB<LIB>_VERSION_MAJOR of each library (avcodec, avutil, ...) from the headers in
#   <include_dir> (version_major.h, or version.h for older trees) and returns a list of
#   compile definitions CPLAY_FFMPEG_EXPECT_<LIB>_MAJOR=<major> in <out_var>.
#   src/utils/ffmpegversioncheck.h turns a mismatch into a compile error.

function(cplay_ffmpeg_header_majors include_dir out_var)
    set(_defs "")
    foreach(_lib IN LISTS ARGN)
        string(TOLOWER "${_lib}" _lower)
        string(TOUPPER "${_lib}" _upper)
        set(_major "")
        foreach(_header version_major.h version.h)
            set(_file "${include_dir}/lib${_lower}/${_header}")
            if(_major STREQUAL "" AND EXISTS "${_file}")
                file(STRINGS "${_file}" _line REGEX "^#define[ \t]+LIB${_upper}_VERSION_MAJOR[ \t]+[0-9]+")
                if(_line)
                    string(REGEX REPLACE "^#define[ \t]+LIB${_upper}_VERSION_MAJOR[ \t]+([0-9]+).*$" "\\1" _major "${_line}")
                endif()
            endif()
        endforeach()
        if(_major STREQUAL "")
            message(FATAL_ERROR "No LIB${_upper}_VERSION_MAJOR in ${include_dir}/lib${_lower}")
        endif()
        list(APPEND _defs "CPLAY_FFMPEG_EXPECT_${_upper}_MAJOR=${_major}")
    endforeach()
    set(${out_var} "${_defs}" PARENT_SCOPE)
endfunction()
