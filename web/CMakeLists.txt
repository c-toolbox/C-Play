#
# Copyright: 2026 Erik Sunden <eriksunden85@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

# Web apps served by the C-Play HTTP server. Each is built with pnpm into its dist folder and
# installed to bin/data/web/<name>, where the server looks for it.
if(BUILD_CPLAY_WEB_VIEWS)
    find_program(PNPM_EXECUTABLE NAMES pnpm.cmd pnpm)
endif()

function(cplay_web_app name description)
    set(_dir "${CMAKE_CURRENT_SOURCE_DIR}/${name}")
    if(BUILD_CPLAY_WEB_VIEWS)
        if(PNPM_EXECUTABLE)
            file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${_dir}/src/*")
            add_custom_command(
                OUTPUT "${_dir}/dist/index.html"
                COMMAND "${PNPM_EXECUTABLE}" install --frozen-lockfile
                COMMAND "${PNPM_EXECUTABLE}" run build
                WORKING_DIRECTORY "${_dir}"
                DEPENDS ${_sources}
                    "${_dir}/index.html"
                    "${_dir}/package.json"
                    "${_dir}/pnpm-lock.yaml"
                    "${_dir}/vite.config.ts"
                COMMENT "Building the ${description} (web/${name})"
                VERBATIM)
            add_custom_target(${name}_web ALL DEPENDS "${_dir}/dist/index.html")
            set_target_properties(${name}_web PROPERTIES FOLDER "Web")
        else()
            message(STATUS "pnpm not found, build web/${name} by hand: pnpm install && pnpm build")
        endif()
    endif()

    install(DIRECTORY "${_dir}/dist/"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data/web/${name}" OPTIONAL)
endfunction()

cplay_web_app(presentation_mode "presenter view")
cplay_web_app(movie_mode "movie view")
