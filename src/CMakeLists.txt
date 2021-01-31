include_directories(${TCPSPSUITE_SOURCE_DIR}/src)
link_directories(${TCPSPSUITE_BINARY_DIR}/)

set(SOLVER_HEADERS "")
set(SOURCES instance/instance.cpp instance/job.cpp instance/resource.cpp
        instance/laggraph.cpp instance/solution.cpp instance/transform.cpp io/jsonreader.cpp
        baselines/earlyscheduler.cpp manager/timer.cpp util/randomizer.cpp
        instance/traits.cpp manager/errors.cpp visualization/dotfile.cpp
        algorithms/graphalgos.cpp util/solverconfig.cpp io/solutionwriter.cpp datastructures/jobset.cpp
        util/configuration.cpp db/db_factory.cpp 
        datastructures/skyline.cpp datastructures/leveltree.cpp
				manager/parallelizer.cpp util/log.cpp
				util/autotuneconfig.cpp util/parameter.cpp
        db/storage.cpp db/db_objects.cpp db/db_objects-odb.cxx
        manager/memoryinfo.cpp util/thread_checker.cpp datastructures/overlapping_jobs_generator.cpp)
			

# The db objects are outside of our control, so we disable warnings for them
set_source_files_properties(db/db_objects-odb.cxx PROPERTIES COMPILE_FLAGS -w)
# ODB uses a lot of explicitly instantiated templates not visible in the respective translation units
set_source_files_properties(db/storage.cpp PROPERTIES COMPILE_FLAGS -Wno-undefined-var-template)

if (${ODB_SQLITE_FOUND})
   set(SOURCES ${SOURCES} db/db_objects-odb-sqlite.cxx db/db_objects-schema-sqlite.cxx)
	 set_source_files_properties(db/db_objects-odb-sqlite.cxx PROPERTIES COMPILE_FLAGS -w)
	 set_source_files_properties(db/db_objects-schema-sqlite.cxx PROPERTIES COMPILE_FLAGS -w)
endif()
if (${ODB_MYSQL_FOUND})
   set(SOURCES ${SOURCES} db/db_objects-odb-mysql.cxx db/db_objects-schema-mysql.cxx)
	 set_source_files_properties(db/db_objects-odb-mysql.cxx PROPERTIES COMPILE_FLAGS -w)
	 set_source_files_properties(db/db_objects-schema-mysql.cxx PROPERTIES COMPILE_FLAGS -w)
endif()

if (INSTRUMENT_MALLOC)
	set(SOURCES ${SOURCES} contrib/tbingmann/malloc_tracker.cpp)
	set(LIBS ${LIBS} dl)
endif()

##########################################################################################
#
# Use the gold linker if available
#
##########################################################################################
set(OLD_FLAGS ${CMAKE_REQUIRED_FLAGS})
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror -fuse-ld=gold")
INCLUDE(CheckCXXSourceCompiles) 
CHECK_CXX_SOURCE_COMPILES(
	"int main() { return 0; } "
	HAS_GOLD_LINKER
	)
if ((HAS_GOLD_LINKER) AND (ALLOW_INCREMENTAL_LINKING))
	set(LINK_FLAGS "${LINK_FLAGS} --incremental")
endif()

set(CMAKE_REQUIRED_FLAGS "${OLD_FLAGS} -Werror -fuse-ld=lld")
INCLUDE(CheckCXXSourceCompiles) 
CHECK_CXX_SOURCE_COMPILES(
	"int main() { return 0; } "
	HAS_LLD_LINKER
)
if ((HAS_LLD_LINKER) AND (ALLOW_INCREMENTAL_LINKING))
	set(LINK_FLAGS "${LINK_FLAGS} --incremental")
endif()

set(CMAKE_REQUIRED_FLAGS "${OLD_FLAGS}")

#if (HAS_LLD_LINKER)
#	message(STATUS " +++ Detected linker: lld")
#	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
#elseif (HAS_GOLD_LINKER)
if ((HAS_GOLD_LINKER) AND (NOT (USE_DEFAULT_LINKER)))
	message(STATUS " +++ Detected linker: gold")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
else()
	message(STATUS " +++ Detected linker: default")
endif()

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
	if (${YGG_STORE_SEQUENCE} OR ${YGG_STORE_SEQUENCE_DST})
		message(WARNING "Not enabling state propagation solver b/c of compatibility problems with YGG_STORE_SEQUENCE!")
	else()
    add_subdirectory(state_propagation)
	endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/grasp/")
    add_subdirectory(grasp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/peak_meta/")
    add_subdirectory(peak_meta)
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/swag/")
    add_subdirectory(swag)
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
set_target_properties(commonlib PROPERTIES
    COTIRE_PREFIX_HEADER_IGNORE_PATH "${CMAKE_SOURCE_DIR}/src/db")
set_target_properties(commonlib PROPERTIES COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)

if (NOT ${CMAKE_EXPORT_COMPILE_COMMANDS})
   cotire(commonlib)
else()   
endif()

# main
add_executable(tcpspsuite $<TARGET_OBJECTS:commonlib> main.cpp)
set_target_properties(tcpspsuite PROPERTIES COMPILE_FLAGS -std=c++17)
set_target_properties(tcpspsuite PROPERTIES COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
target_link_libraries(tcpspsuite ${LIBS})
if (NOT ${CMAKE_EXPORT_COMPILE_COMMANDS})
   cotire(tcpspsuite)
endif()

# autotune
add_executable(autotune $<TARGET_OBJECTS:commonlib> autotune.cpp)
target_link_libraries(autotune ${LIBS})
set_target_properties(autotune PROPERTIES COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
if (NOT ${CMAKE_EXPORT_COMPILE_COMMANDS})
   cotire(autotune)
endif()

# DB Merger
add_executable(db_merger $<TARGET_OBJECTS:commonlib> tools/dbmerger.cpp)
target_link_libraries(db_merger ${LIBS})
set_target_properties(db_merger PROPERTIES COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
if (NOT ${CMAKE_EXPORT_COMPILE_COMMANDS})
   cotire(db_merger)
endif()

# Completeness Checker
add_executable(completeness_checker $<TARGET_OBJECTS:commonlib> tools/completeness_checker.cpp)
target_link_libraries(completeness_checker ${LIBS})
set_target_properties(completeness_checker PROPERTIES COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
if (NOT ${CMAKE_EXPORT_COMPILE_COMMANDS})
   cotire(completeness_checker)
endif()
