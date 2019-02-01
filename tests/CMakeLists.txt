add_subdirectory(gtest)

find_package( Threads )

if (INSTRUMENT_MALLOC)
	set(LIBS ${LIBS} dl)
endif()

include_directories(SYSTEM ${GTEST_INCLUDE_DIRS})

# main
add_executable(run_tests $<TARGET_OBJECTS:commonlib> main.cpp)
add_dependencies(run_tests googletest)
set_target_properties(run_tests PROPERTIES COMPILE_FLAGS -std=c++17)

# Everything else should be in here
target_link_libraries(run_tests ${LIBS} ${GTEST_LIBS_DIR}/libgtest.a
        ${GTEST_LIBS_DIR}/libgtest_main.a ${CMAKE_THREAD_LIBS_INIT})
if (NOT ${CMAKE_EXPORT_COMPILE_COMMANDS})
   cotire(run_tests)
endif()
