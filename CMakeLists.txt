cmake_minimum_required(VERSION 3.0)

# Since this is only available in cmake 3.9, we set -std=c++1z manually below
#set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

include(cotire)
include(FeatureSummary)

project(TCPSPSUITE)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

# Find Cairo
find_package(Cairo)
include_directories(${Cairo_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Cairo_LIBRARIES})

# Find Boost
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
find_package(Boost REQUIRED COMPONENTS system thread log program_options)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})

# Threading support
find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

# Google SparseHash
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/contrib/sparsehash)

# Find SQlite
find_package(Sqlite REQUIRED)
include_directories(${SQLITE_INCLUDE_DIRS})
set(LIBS ${LIBS} ${SQLITE_LIBRARIES})

# Find ODB
find_package(ODB REQUIRED
        COMPONENTS sqlite)
include(${ODB_USE_FILE})
#include_directories(SYSTEM ${ODB_INCLUDE_DIRS})
#include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/src/contrib/odb/include)
set(LIBS ${LIBS} ${ODB_LIBRARIES})

#
# ODB is major PITA. Since they use throw(…) (which is deprecated for ages and thrown out of
# C++17), we need to patch the headers, or it won't compile with --std=c++17. Therefore, we
# first copy the ODB_INCLUDE_DIRS to some known location, and then apply a patch to them.
#
message(STATUS "================ ODB Patching ==============================")
message(STATUS "Copying ODB headers from ${ODB_INCLUDE_DIRS} to build tree…")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src/contrib/odb_headers")
file(COPY "${ODB_INCLUDE_DIRS}/odb" DESTINATION src/contrib/odb_headers)

file(GLOB_RECURSE odb_files LIST_DIRECTORIES false
        "${CMAKE_CURRENT_BINARY_DIR}/src/contrib/odb_headers/*")
foreach(file ${odb_files})
    file(READ ${file} filedata)

    string(REGEX REPLACE "throw \\(\\)" "noexcept" filedata "${filedata}")
    string(REGEX REPLACE "throw \\(std::bad_alloc\\)" "" filedata "${filedata}")

    file(WRITE ${file} "${filedata}")
endforeach()

message(STATUS "Adding ${CMAKE_CURRENT_BINARY_DIR}/src/contrib/odb_headers to include directories.")
include_directories(SYSTEM ${CMAKE_CURRENT_BINARY_DIR}/src/contrib/odb_headers)
message(STATUS "================  END OF ODB Patching =======================")


# Find Gurobi
find_package(GUROBI)
if (GUROBI_FOUND)
  include_directories(${GUROBI_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${GUROBI_CXX_LIBRARY} ${GUROBI_LIBRARIES} ${GUROBI_CXX_LIBRARY})
endif(GUROBI_FOUND)

# Find CPLEX
find_package(CPLEX)
if (CPLEX_FOUND)
  include_directories(SYSTEM ${CPLEX_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${CPLEX_LIBRARIES})
endif(CPLEX_FOUND)

# One might one day turn on -Waggregate-return…

# Set a default build type if none was specified
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
        "MinSizeRel" "RelWithDebInfo" "Profile" "Instrument" "Valgrind")

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
endif()

#===============================================
#==== Build type: default
#===============================================

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/src/db/db_objects \
 -isystem ${CMAKE_CURRENT_SOURCE_DIR}/src/contrib/json")
set(CMAKE_CXX_FLAGS "-std=c++17 ${CMAKE_C_FLAGS}")

#===============================================
#==== Build type: debug
#===============================================

set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer -Wall -Wextra -pedantic -Wfloat-equal \
-Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings -Wcast-qual \
-Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wfloat-equal -Wswitch-default")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

#===============================================
#==== Build type: instrument
#===============================================

set(CMAKE_C_FLAGS_INSTRUMENT "-g -O0 -fno-omit-frame-pointer -Wall -Wextra -pedantic \
-Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings \
-Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wfloat-equal \
-Wswitch-default -fstack-protector -fsanitize=undefined -ftrapv -fno-sanitize-recover \
-fsanitize=address -fsanitize-undefined-trap-on-error")
set(CMAKE_CXX_FLAGS_INSTRUMENT "${CMAKE_C_FLAGS_INSTRUMENT}")

#===============================================
#==== Build type: threadinstrument
#===============================================

set(CMAKE_C_FLAGS_THREADINSTRUMENT "-g -O0 -fno-omit-frame-pointer -Wall -Wextra -pedantic -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wfloat-equal -Wswitch-default -fstack-protector -fsanitize=undefined -ftrapv -fno-sanitize-recover -fsanitize=thread")
set(CMAKE_CXX_FLAGS_THREADINSTRUMENT "${CMAKE_C_FLAGS_INSTRUMENT}")

#===============================================
#==== Build type: release
#===============================================

# TODO fast math? Hum…
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -ffast-math -flto")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")

#===============================================
#==== Build type: profile
#===============================================

# TODO fast math? Hum…
set(CMAKE_C_FLAGS_PROFILE "-O3 -march=native -ffast-math -pg" CACHE STRING
    "Flags used by the C++ compiler during profile builds."
    FORCE)
set(CMAKE_CXX_FLAGS_PROFILE "${CMAKE_C_FLAGS_PROFILE}" CACHE STRING
    "Flags used by the C compiler during profile builds."
    FORCE )
set (CMAKE_SHARED_LINKER_FLAGS_PROFILE "${CMAKE_SHARED_LINKER_FLAGS} -pg")
set (CMAKE_EXE_LINKER_FLAGS_PROFILE "${CMAKE_EXE_LINKER_FLAGS} -pg")
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_PROFILE
    CMAKE_C_FLAGS_PROFILE
    CMAKE_EXE_LINKER_FLAGS_PROFILE
    CMAKE_SHARED_LINKER_FLAGS_PROFILE )

#===============================================
#==== Build type: valgrind
#===============================================

set(CMAKE_C_FLAGS_VALGRIND "-O2 -march=native -ffast-math -g -flto" CACHE STRING
        "Flags used by the C++ compiler during valgrind builds."
        FORCE)
set(CMAKE_CXX_FLAGS_VALGRIND "${CMAKE_C_FLAGS_VALGRIND}" CACHE STRING
        "Flags used by the C compiler during valgrind builds."
        FORCE )
set (CMAKE_SHARED_LINKER_FLAGS_VALGRIND "${CMAKE_SHARED_LINKER_FLAGS}")
set (CMAKE_EXE_LINKER_FLAGS_VALGRIND "${CMAKE_EXE_LINKER_FLAGS}")
MARK_AS_ADVANCED(
        CMAKE_CXX_FLAGS_VALGRIND
        CMAKE_C_FLAGS_VALGRIND
        CMAKE_EXE_LINKER_FLAGS_VALGRIND
        CMAKE_SHARED_LINKER_FLAGS_VALGRIND )


#SET( CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
#    "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Profile."
#    FORCE )

#
# User Configuration
#

SET(SOFT_FAIL "FALSE" CACHE BOOL "Try to fail softly whenever possible")
SET(ENABLE_CONSISTENCY_CHECKS "TRUE" CACHE BOOL "Enable consistency checks")
SET(ENABLE_ASSERTIONS "TRUE" CACHE BOOL "Enable assertions")
SET(OMG_VERIFY "FALSE" CACHE BOOL "Verify over 9000")
SET(VERIFY_NUMERICS "FALSE" CACHE BOOL "Verify assumptions about numbers")
SET(CRASH_ON_CHECK "TRUE" CACHE BOOL "Crash the program when verification fails. Use this for debugging in gdb.")
SET(CATCH_EXCEPTIONS "TRUE" CACHE BOOL "Try to catch everything and provide own crash handler")
SET(EXIT_ON_SIGINT "FALSE" CACHE BOOL "Exit cleanly when interrupted via CTRL-C.")
SET(NUMA_OPTIMIZE "FALSE" CACHE BOOL "Optimize for performance on NUMA enabled systems")

SET(MAX_DBG_LEVEL 5 CACHE INT "Maximum debug level")

#
# Configure config.in
#
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/src/generated_config.hpp" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/config.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/tests/generated_config.hpp" @ONLY)
include_directories("${PROJECT_BINARY_DIR}/src")

add_subdirectory(src)

#
# Optional: Testing
#
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/")
    add_subdirectory(tests)
endif()

set_package_properties(IPOPT PROPERTIES TYPE OPTIONAL PURPOSE "Required for the 'closed form' optimization algorithm")
set_package_properties(GiNaC PROPERTIES TYPE OPTIONAL PURPOSE "Required for the 'closed form' optimization algorithm")
set_package_properties(GUROBI PROPERTIES TYPE OPTIONAL PURPOSE "Required for the ILP optimization\
 algorithm (using the Gurobi solver)")
set_package_properties(CPLEX PROPERTIES TYPE OPTIONAL PURPOSE "Required for the ILP optimization \
algorithm (using the CPLEX solver)")
set_package_properties(Cairo PROPERTIES TYPE OPTIONAL PURPOSE "Required to draw a visualization of the result")
set_package_properties(GTest PROPERTIES TYPE OPTIONAL PURPOSE "Required to build unit tests")

message(STATUS "=====================================================")
message(STATUS "========           Optional Packages         ========")
message(STATUS "=====================================================")
message(STATUS "The following optional packages were found:")
FEATURE_SUMMARY(WHAT OPTIONAL_PACKAGES_FOUND)
message(STATUS "=====================================================")
message(STATUS "The following optional packages were NOT found:")
FEATURE_SUMMARY(WHAT OPTIONAL_PACKAGES_NOT_FOUND)
message(STATUS "=====================================================")
