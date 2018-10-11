@page solvers Available Solvers

This page lists the solvers that are available to optimize instances. Not all solvers can optimize all kinds of instances. We capture this in the form of *requirements* of a solver: If a solver has a certain requirement, all instances that should be optimized by this solver must fulfill the requirement. The currently possible requirements are listed in the requirement documentation TODO.

Every solver has a certain ID, which can be used to select it in @ref solver_configurations and from the command line. Of some solvers, multiple versions are available. In this case, the solver ID is extended in the form `SolverID<VersionInformation>`.

### Mixed-Integer Linear Program

* **ID**: `ILPBase v2.3 <MIPSolver>`
 * Possible values for `MIPSolver`: `Gurobi`, `CPLEX`
* **Software Requirements**: Either CPLEX and/or Gurobi must be installed and found. See @ref gettingstarted for details. 
* **Instance Requirements**: None

The Mixed-Integer Programming solver uses the MIP presented in [1] and [2] to optimize TCPSP instances.

#### Possible Configuration
* **dump_path**: A file path that the created model is written to in "LP" format. Note that if you specified multiple instances to be solved, they will overwrite each other.
* **dump\_solution_path**: A file path that the solved model will be written to. Note that if you specified multiple instances to be solved, they will overwrite each other.
* **use\_sos1\_for_starts**: Binary option. Set to `true` to use SOS1 type constraints instead of a simple sum to enforce exactly one start time to be selected per job. Defaults to `false`.
* **initialize\_with_early**: Binary option. Set to `true` to use the EarlyScheduler to compute a first feasible solution that the MIP solver is warm-started with. Defaults to `true`.
* **focus**: Integer option. Asks the MIP solver used (Gurobi / CPLEX) to focus on one of the following:
  * **0**: Balanced optimization, do not focus on anything in particular. (Default)
  * **1**: Focus on finding solutions with good quality.
  * **2**: Focus on finding and proving better lower bounds.
  * **3**: Focus on showing optimality of the found solutions.



References
----------

[1] Lukas Barth, Nicole Ludwig, Esther Mengelkamp, and Philipp Staudt. 2017. A comprehensive modelling framework for demand side flexibility in smart grids. Computer Science - Research and Development (30 Aug 2017). https://doi.org/10.1007/s00450-017-0343-x

[2] Lukas Barth, Veit Hagenmeyer, Nicole Ludwig, and Dorothea Wagner. 2018.
How much demand side flexibility do we need? Analyzing where to exploit
flexibility in industrial processes. In Proceedings of ACM eEnergy Conference (eEnergy’18). ACM, New York, NY, USA, 22 pages.
