# - Try to find PORTAUDIO
# Once done, this will define
#
#  PORTAUDIO_FOUND - system has PortAudio
#  PORTAUDIO_INCLUDE_DIRS - the PortAudio include directories
#  PORTAUDIO_LIBRARIES - link these to use PortAudio

# Library
find_library(PORTAUDIO_LIBRARY
	NAMES portaudio libportaudio
	PATH_SUFFIXES lib
	PATHS ${VCPKG_ROOT}/installed/x64-windows ${PORTAUDIO_ROOT}
)

if(PORTAUDIO_LIBRARY)
	get_filename_component(PORTAUDIO_DIR "${PORTAUDIO_LIBRARY}" DIRECTORY)
  cmake_path(GET PORTAUDIO_DIR PARENT_PATH PORTAUDIO_LIB_PARENT)
endif()

# Include headers (try to prioritze to find it in VCPKG_ROOT)
find_path(PORTAUDIO_INCLUDE_DIR
	NAMES portaudio.h
	PATH_SUFFIXES include includes include/portaudio
	PATHS ${VCPKG_ROOT}/installed/x64-windows ${PORTAUDIO_ROOT} ${PORTAUDIO_LIB_PARENT}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PortAudio
  FOUND_VAR PORTAUDIO_FOUND
  REQUIRED_VARS
    PORTAUDIO_LIBRARY
    PORTAUDIO_INCLUDE_DIR
)

if(PORTAUDIO_FOUND)
  set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARY})
  set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})

  if (NOT TARGET PortAudio::PortAudio)
    add_library(PortAudio::PortAudio UNKNOWN IMPORTED)
    set_target_properties(PortAudio::PortAudio PROPERTIES
		IMPORTED_LOCATION "${PORTAUDIO_LIBRARIES}"
		IMPORTED_LOCATION_RELEASE "${PORTAUDIO_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${PORTAUDIO_INCLUDE_DIRS}"
    )
   endif()
endif()

mark_as_advanced(PORTAUDIO_LIBRARY PORTAUDIO_LIBRARIES PORTAUDIO_INCLUDE_DIR PORTAUDIO_INCLUDE_DIRS PORTAUDIO_DIR)