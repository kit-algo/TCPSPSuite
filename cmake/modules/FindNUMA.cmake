include(FindPackageHandleStandardArgs)

find_path(NUMA_PATH_DIR
  NAMES include/numa.h
  PATHS ENV NUMA_PATH
  DOC "Prefix for NUMA paths")

find_path(NUMA_INCLUDE_DIR
  NAMES numa.h
  HINTS ${NUMA_PATH_DIR}
  PATH_SUFFIXES include
  DOC "NUMA includes directory")

find_library(NUMA_LIBRARY
  NAMES numa
  HINTS ${NUMA_PATH_DIR}
  DOC "NUMA library")

if (NUMA_LIBRARY)
    get_filename_component(NUMA_LIBRARY_DIR ${NUMA_LIBRARY} PATH)
endif()

mark_as_advanced(NUMA_INCLUDE_DIR NUMA_LIBRARY_DIR NUMA_LIBRARY)

find_package_handle_standard_args(NUMA REQUIRED_VARS NUMA_PATH_DIR NUMA_INCLUDE_DIR NUMA_LIBRARY)