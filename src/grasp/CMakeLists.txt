message(">>> Registering GRASP solver for build")

set(SOURCES ${SOURCES} grasp/grasp.cpp PARENT_SCOPE)
set(SOLVER_HEADERS "${SOLVER_HEADERS}#include \"grasp/grasp.hpp\" \n"
        PARENT_SCOPE)