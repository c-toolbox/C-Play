#
# SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>
# SPDX-FileCopyrightText: 2019 Heiko Becker <heirecka@exherbo.org>
# SPDX-FileCopyrightText: 2020 Elvis Angelaccio <elvis.angelaccio@kde.org>
# SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
# FindLibmpv
# ----------
#
# Find the mpv media player client library.
#
# Defines the following variables:
#
# - Libmpv_FOUND
#     True if it finds the library and include directory
#
# - Libmpv_INCLUDE_DIRS
#     The libmpv include dirs for use with target_include_directories
#
# - Libmpvb_LIBRARIES
#     The libmpv libraries for use with target_link_libraries()
#
# - Libmpv_VERSION
#     The version of the found libmpv
#
#
# Defines the following imported target if 'Libmpv_FOUND' is true:
#
# - Libmpv::Libmpv
#

find_package(PkgConfig QUIET)

pkg_search_module(PC_MPV QUIET mpv)

find_library(Libmpv_LIBRARIES
    NAMES mpv
    HINTS $ENV{PC_MPV_LIBDIR}
)

find_path(Libmpv_INCLUDE_DIRS
    NAMES client.h
    PATH_SUFFIXES mpv
    HINTS $ENV{PC_MPV_INCLUDEDIR}
)

if(Libmpv_LIBRARIES AND NOT Libmpv_INCLUDE_DIRS)
    get_filename_component(LIBMPV_LIBDIR ${Libmpv_LIBRARIES} PATH)
    get_filename_component(LIBMPV_ROOT_DIR ${LIBMPV_LIBDIR} DIRECTORY)
    find_path(Libmpv_INCLUDE_DIRS
        NAMES client.h
        PATH_SUFFIXES mpv
        PATHS "${LIBMPV_ROOT_DIR}/include"
    )
endif()

set(Libmpv_VERSION ${PC_MPV_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libmpv
    FOUND_VAR
        Libmpv_FOUND
    REQUIRED_VARS
        Libmpv_LIBRARIES
        Libmpv_INCLUDE_DIRS
    VERSION_VAR
        Libmpv_VERSION
)

if (Libmpv_FOUND AND NOT TARGET Libmpv::Libmpv)
    add_library(Libmpv::Libmpv UNKNOWN IMPORTED)
    set_target_properties(Libmpv::Libmpv PROPERTIES
        IMPORTED_LOCATION "${Libmpv_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${Libmpv_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(Libmpv_LIBRARIES Libmpv_INCLUDE_DIRS)

include(FeatureSummary)
set_package_properties(Libmpv PROPERTIES
    URL "https://mpv.io"
    DESCRIPTION "mpv media player client library"
)
