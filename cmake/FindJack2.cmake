# - Try to find JACK2
# Once done, this will define
#
#  JACK2_FOUND - system has JACK
#  JACK2_INCLUDE_DIRS - the JACK include directories
#  JACK2_LIBRARIES - link these to use JACK

find_program(JACK2_PROGRAM NAMES jackd.exe PATHS $ENV{ProgramFiles}/JACK2)

if(JACK2_PROGRAM)
	get_filename_component(JACK2_DIR "${JACK2_PROGRAM}" DIRECTORY)
endif()

# Include dir
find_path(JACK2_INCLUDE_DIR
	NAMES jack/jack.h
	PATH_SUFFIXES include includes
	PATHS ${JACK2_DIR}
)

# Library
find_library(JACK2_LIBRARY
	NAMES libjack64
	PATH_SUFFIXES lib
	PATHS ${JACK2_DIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jack2
  FOUND_VAR JACK2_FOUND
  REQUIRED_VARS
    JACK2_LIBRARY
    JACK2_INCLUDE_DIR
)

if(JACK2_FOUND)
  set(JACK2_LIBRARIES ${JACK2_LIBRARY})
  set(JACK2_INCLUDE_DIRS ${JACK2_INCLUDE_DIR})

  if (NOT TARGET Jack2::Jack)
    add_library(Jack2::Jack UNKNOWN IMPORTED)
    set_target_properties(Jack2::Jack PROPERTIES
		IMPORTED_LOCATION "${JACK2_LIBRARIES}"
		IMPORTED_LOCATION_RELEASE "${JACK2_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${JACK2_INCLUDE_DIRS}"
    )
   endif()
endif()

mark_as_advanced(JACK2_LIBRARY JACK2_LIBRARIES JACK2_INCLUDE_DIR JACK2_INCLUDE_DIRS JACK2_DIR)