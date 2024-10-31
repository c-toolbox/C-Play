if(WIN32)
  if(DEFINED ENV{NDI_SDK_DIR})
    set(NDI_FOUND TRUE)
    set(NDI_DIR $ENV{NDI_SDK_DIR})
    string(REPLACE "\\" "/" NDI_DIR "${NDI_DIR}")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(NDI_ARCH "x86")
    else()
      set(NDI_ARCH "x64")
    endif()
    set(NDI_INCLUDE_DIR "${NDI_DIR}/Include")
    set(NDI_LIBRARY_DIR "${NDI_DIR}/Lib/${NDI_ARCH}")
    set(NDI_BINARY_DIR "${NDI_DIR}/Bin/${NDI_ARCH}")
    set(NDI_LIBS "Processing.NDI.Lib.${NDI_ARCH}")
  else()
    set(NDI_FOUND FALSE)
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NDI DEFAULT_MSG NDI_DIR ${NDI_FOUND})

find_path(NDI_INCLUDE
  NAMES
    Processing.NDI.compat.h
    Processing.NDI.deprecated.h
    Processing.NDI.DynamicLoad.h
    Processing.NDI.Find.h
    Processing.NDI.FrameSync.h
    Processing.NDI.Lib.cplusplus.h
    Processing.NDI.Lib.h
    Processing.NDI.Recv.ex.h
    Processing.NDI.Recv.h
    Processing.NDI.Routing.h
    Processing.NDI.Send.h
    Processing.NDI.structs.h
    Processing.NDI.utilities.h
  PATHS "${NDI_INCLUDE_DIR}"
)

find_library(NDI_LIBRARY
  NAMES ${NDI_LIBS}
  PATHS "${NDI_LIBRARY_DIR}"
)
if(NOT NDI_LIBRARY)
  message(FATAL_ERROR "NDI SDK: ${NDI_LIBS}.lib not found in:\n${NDI_LIBRARY_DIR}\nMaybe you have to create a symlink or rename the file.")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NDI
  FOUND_VAR NDI_FOUND
  REQUIRED_VARS
    NDI_LIBRARY
    NDI_INCLUDE
)

if(NDI_FOUND AND NOT TARGET NDI::NDI)
  add_library(NDI::NDI UNKNOWN IMPORTED)
  set_target_properties(NDI::NDI PROPERTIES
    IMPORTED_LOCATION "${NDI_LIBRARY}"
    IMPORTED_LOCATION_RELEASE "${NDI_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${NDI_INCLUDE}"
  )
endif()

mark_as_advanced(
  NDI_INCLUDE
  NDI_LIBRARY
)
