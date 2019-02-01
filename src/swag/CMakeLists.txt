message(">>> Registering EdgeInsertion solver for build")

set(SOURCES ${SOURCES} swag/swag.cpp swag/matrixedgescorer.cpp swag/elitepoolscorer.cpp PARENT_SCOPE)
set(SOLVER_HEADERS "${SOLVER_HEADERS}#include \"swag/swag.hpp\" \n"
        PARENT_SCOPE)
