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

### SWAG

* **ID**: `SWAG v.1.0`
* **Software Requirements**: None
* **Instance Requirements**: Only finish-start dependencies, no drain, no window extension, no availability, no overshoot costs

This is the Scheduling with Augmented Graphs (SWAG) algorithm presented in [3]. It is an iterative heuristic
that reduces peak demand by repeatedly inserting new dependencies between jobs executing concurrently during peak demand.

#### Possible Configuration
Refer to [3] for the meaning of the parameters **deletions_before_reset**, **deletion_trials**, **complete_propagation_after**, **deletion_max_depth**, **edge_candidate_batchsize** and **deletion_undermove_penalty**.

Other parameters not mentioned in that paper are:

* **disaggregate_time**: Boolean option. Controls whether to write detailed statistics to the database, incurs a small performance penalty.

TODO document scorers


### GRASP

* **ID**: `GRASP<A,B>`
  * `A` indicates which method is being used to determine the demand during the algorithm, with the possible values being `array` for a simple time-indexed array implementation and `skyline` for tha dynamic segment tree - based implementation. We recommend `skyline`.
  * `B` indicates whether the `random` or `sorted` variant of the algorithm is used (cf. [4])
* **Software Requirements**: None
* **Instance Requirements**: Only positive lags, no drain, no window extension, no availability, no overshoot costs

This is the Greedy Randomized Adaptive Search Procedure introduced by Petersen et al. [4], a metaheuristic approach.

#### Possible Configuration

* **weightedIterationss**: How many times should the local search (Algorithm 1 in [4]) be executed using the *weighted* selection strategy?
* **uniformIterations**: How many times should the local search (Algorithm 1 in [4]) be executed using the *uniform* selection strategy?
* **uniformSelections**: The n in line 1 of Algorithm 2 of [4]. Note that in [4], the authors always use the number of jobs for this value.
* **weightedSelections**: The n in line 1 of Algorithm 3 of [4]. Note that in [4], the authors always use the number of jobs for this value.
* **graspSelection**: The m in line 3 of Algorithm 4 resp. line Algorithm 5 in [4].
* **graspSamples**: The l in Algorithm 4 resp. Algorithm 5 in [4].
* **resetCount**: Allows to start the algorithm with a clean slate after a certain number of iterations. The global best solution is always preserved.

References
----------


[1] Lukas Barth, Nicole Ludwig, Esther Mengelkamp, and Philipp Staudt. 2017. A comprehensive modelling framework for demand side flexibility in smart grids. Computer Science - Research and Development (30 Aug 2017). https://doi.org/10.1007/s00450-017-0343-x

[2] Lukas Barth, Veit Hagenmeyer, Nicole Ludwig, and Dorothea Wagner. 2018.
How much demand side flexibility do we need? Analyzing where to exploit
flexibility in industrial processes. In Proceedings of ACM eEnergy Conference (eEnergy’18). ACM, New York, NY, USA, 22 pages.

[3] Lukas Barth and Dorothea Wagner. 2019. Shaving Peaks by Augmenting the
Dependency Graph. In Proceedings of the Tenth ACM International Conference
on Future Energy Systems (e-Energy ’19), June 25–28, 2019, Phoenix, AZ,
USA. ACM, New York, NY, USA, Article 4, 11 pages. https://doi.org/10.1145/3307772.3328298

[4] M. K. Petersen, L. H. Hansen, J. Bendtsen, K. Edlund, and J. Stoustrup, 
“Heuristic Optimization for the Discrete Virtual Power Plant Dispatch Problem,” 
IEEE Transactions on Smart Grid, vol. 5, no. 6, pp. 2910–2918, Nov. 2014.
