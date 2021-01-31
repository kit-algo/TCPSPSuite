message(">>> Registering ILP solver for build")

set(ILP_HEADERS "")

if(GUROBI_FOUND)
    set(ILP_HEADERS "${ILP_HEADERS} #include \"contrib/ilpabstraction/src/ilpa_gurobi.hpp\"\n")
endif()

if(CPLEX_FOUND)
    set(ILP_HEADERS "${ILP_HEADERS} #include \"contrib/ilpabstraction/src/ilpa_cplex.hpp\"\n")
endif()

if(GUROBI_FOUND OR CPLEX_FOUND)
    set(ILP_HEADERS "${ILP_HEADERS} #include \"ilp/dtilp.hpp\"\n #include \"ilp/ebilp.hpp\"\n #include \"ilp/obilp.hpp\"\n #include \"ilp/fbilp.hpp\"\n")

    set(SOURCES ${SOURCES} ilp/ilp.cpp ilp/dtilp.cpp ilp/ebilp.cpp ilp/obilp.cpp ilp/fbilp.cpp PARENT_SCOPE)
    set(SOLVER_HEADERS "${SOLVER_HEADERS} ${ILP_HEADERS}"
            PARENT_SCOPE)
endif()
