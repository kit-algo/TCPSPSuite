@page solvers Available Solvers

This page lists the solvers that are available to optimize instances. Not all solvers can optimize all kinds of instances. We capture this in the form of *requirements* of a solver: If a solver has a certain requirement, all instances that should be optimized by this solver must fulfill the requirement. The currently possible requirements are listed in the requirement documentation TODO.

Every solver has a certain ID, which can be used to select it in solver configurations TODO and from the command line. Of some solvers, multiple versions are available. In this case, the solver ID is extended in the form `SolverID<VersionInformation>`.

### Mixed-Integer Program

* **ID**: `ILPBase v2.3 <MIPSolver>`
 * Possible values for `MIPSolver`: `Gurobi`, `CPLEX`
* **Requirements**: None

The Mixed-Integer Programming solver uses the MIP presented in [1] and [2] to optimize TCPSP instances.

References
----------

[1] Lukas Barth, Nicole Ludwig, Esther Mengelkamp, and Philipp Staudt. 2017. A comprehensive modelling framework for demand side flexibility in smart grids. Computer Science - Research and Development (30 Aug 2017). https://doi.org/10.1007/s00450-017-0343-x

[2] Lukas Barth, Veit Hagenmeyer, Nicole Ludwig, and Dorothea Wagner. 2018.
How much demand side flexibility do we need? Analyzing where to exploit
flexibility in industrial processes. In Proceedings of ACM eEnergy Conference (eEnergy’18). ACM, New York, NY, USA, 22 pages.
