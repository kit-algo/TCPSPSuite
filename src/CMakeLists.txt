include_directories(${TCPSPSUITE_SOURCE_DIR}/src)
link_directories(${TCPSPSUITE_BINARY_DIR}/)

set(SOLVER_HEADERS "")
set(SOURCES instance/instance.cpp instance/job.cpp instance/resource.cpp
        instance/laggraph.cpp instance/solution.cpp instance/transform.cpp io/jsonreader.cpp
        baselines/earlyscheduler.cpp manager/timer.cpp util/randomizer.cpp
        util/git.cpp instance/traits.cpp manager/errors.cpp visualization/dotfile.cpp
        algorithms/graphalgos.cpp util/solverconfig.cpp io/solutionwriter.cpp datastructures/jobset.cpp
        util/configuration.cpp
        datastructures/skyline.cpp datastructures/leveltree.cpp
		manager/parallelizer.cpp util/log.cpp
		util/autotuneconfig.cpp util/parameter.cpp
        db/storage.cpp db/db_objects.cpp db/db_objects-odb.cxx
        db/db_objects-schema-proxy.cpp)

##########################################################################################
#
# Solvers: The solvers are modular s.t. they can easily be removed / disabled. We
# include their directories here if they exist. The contained CMakeLists.txt should
# modify the SOURCES / LIBS / SOLVER_HEADERS variable accordingly.
#
##########################################################################################

message(STATUS "=====================================================")
message(STATUS "========           Enabled Solvers          ========")
message(STATUS "=====================================================")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/state_propagation/")
    add_subdirectory(state_propagation)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/grasp/")
    add_subdirectory(grasp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/peak_meta/")
    add_subdirectory(peak_meta)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bnb/")
    add_subdirectory(bnb)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/moveheuristic/")
    add_subdirectory(moveheuristic)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pdm/")
    add_subdirectory(pdm)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ilp/")
    add_subdirectory(ilp)
endif()

message(STATUS "=====================================================")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/solver_headers.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/solver_headers.hpp" @ONLY)

# Optional: the visualizer
if(CAIRO_FOUND)
  list(APPEND SOURCES "visualization/visualizer.cpp")
endif(CAIRO_FOUND)


# Get git information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/util/git.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/util/git.cpp" @ONLY)
list(APPEND SOURCES "${CMAKE_CURRENT_BINARY_DIR}/util/git.cpp")

#
# The common library (not really a library…). Everything should link against this
#
add_library(commonlib OBJECT ${SOURCES})
set_target_properties(commonlib PROPERTIES COMPILE_FLAGS -std=c++17)
cotire(commonlib)

# main
add_executable(tcpspsuite $<TARGET_OBJECTS:commonlib> main.cpp)
set_target_properties(tcpspsuite PROPERTIES COMPILE_FLAGS -std=c++17)
target_link_libraries(tcpspsuite ${LIBS})
cotire(tcpspsuite)

# autotune
add_executable(autotune $<TARGET_OBJECTS:commonlib> autotune.cpp)
target_link_libraries(autotune ${LIBS})
cotire(autotune)


