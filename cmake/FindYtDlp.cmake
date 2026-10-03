# Locates the yt-dlp executable used by mpv's ytdl_hook to resolve YouTube streams.
#
# A PyInstaller onedir build (a folder containing yt-dlp.exe + _internal/) is supported:
# point YTDLP_ROOT at that folder or directly at the executable inside it. When nothing
# is set, PATH and the default system locations are searched.
#
# Result variables:
#   YtDlp_FOUND        true when the executable was located
#   YtDlp_EXECUTABLE   full path to yt-dlp(.exe)
#   YtDlp_DIR          directory containing the executable (for an onedir build this is
#                      the folder holding yt-dlp.exe + _internal/)

set(YTDLP_ROOT "" CACHE PATH "Folder containing yt-dlp(.exe), or the executable itself")

if(YTDLP_ROOT)
    file(TO_CMAKE_PATH "${YTDLP_ROOT}" YTDLP_ROOT)
    if(IS_DIRECTORY "${YTDLP_ROOT}")
        find_program(YtDlp_EXECUTABLE NAMES yt-dlp.exe yt-dlp HINTS "${YTDLP_ROOT}" NO_DEFAULT_PATH)
    elseif(EXISTS "${YTDLP_ROOT}")
        # Pointed directly at the executable (e.g. an onedir build's yt-dlp/yt-dlp.exe).
        get_filename_component(_ytdlp_dir "${YTDLP_ROOT}" DIRECTORY)
        find_program(YtDlp_EXECUTABLE NAMES yt-dlp.exe yt-dlp HINTS "${_ytdlp_dir}" NO_DEFAULT_PATH)
    endif()
endif()

if(NOT YtDlp_EXECUTABLE)
    find_program(YtDlp_EXECUTABLE NAMES yt-dlp.exe yt-dlp)
endif()

if(YtDlp_EXECUTABLE)
    get_filename_component(YtDlp_DIR "${YtDlp_EXECUTABLE}" DIRECTORY)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(YtDlp
    FOUND_VAR YtDlp_FOUND
    REQUIRED_VARS YtDlp_EXECUTABLE
)

mark_as_advanced(YTDLP_ROOT YtDlp_EXECUTABLE YtDlp_DIR)