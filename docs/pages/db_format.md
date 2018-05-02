@page dbformat Result Database Format

Output is written to a SQLite3 database.

Database Format
---------------

If you are interested in computational results, the most important table in the
database is the `DBResult` table. Every successful run of a solver creates a row
in this table. Almost all other tables contain additional information related
to results and reference into the `DBResult` table.

### DBResult

Each row in this table describes the result of one successful run of a solver. The fields of the table are:

* `run`: The run ID that the solver was run with. This string has no meaning and can be used by users to identify their results. See the invocation options for more details. [TODO LINK]
* `instance`: The unique ID of the instance that this result belongs to. See the @ref instancefileformat "format description of the instance files" for how to retrieve the ID of an instance.
* `score`: The score of the result.
* `algorithm`: Which solver was used for optimization. See the @ref solvers "documentation on available solvers" for details.
* `seed`: The seed value that the solver was started with. This can be used to reproduce the result. See invocation options for details. [TODO]
* `optimal`: Indicates whether the solver ran to optimality or was interrupted because of e.g. a time limit.
* `feasible`: Indicates whether the result the solver computed was feasible. Most solvers only compute feasible results.
* `lower_bound`: In case that the solver did not run to optimality, this indicates the best found lower bound. The value of the `score` field is the best found upper bound (i.e., the best feasible solution found). The gap between the two is the area in which the optimal solution must lie.
* `elapsed`: The time (in seconds) that the optimization took. For results that did not run to optimality, the value will always be around 2700 seconds.
* `cfg`: Foreign Key into the `DBConfig` table. Links this result to the solver configuration (see the solver configuration documentation for details [TODO]) that the solver for this result was run with.

### DBConfig

This table stores all solver configurations (see TODO for details) that solvers were run with. Most of the actual data is stored in the `DBConfigKV` table, which references into the `DBConfig` table. Fields:

* `name`: The name of this solver configuration
* `time_limit`: The time limit set for the solver

### DBConfigKV

This table stores key-value mappings for the solver configurations stored in the database. Every row corresponds to one setting in a solver configuration. Fields:

* `cfg`: Foreign Key into the `DBConfig` table. Indicates which solver configuration this mapping belongs to.
* `key`: The key of this setting
* `value`: The value of this setting

### DBSolution

If solutions (i.e., an assignment of start times to jobs) should be stored (see TODO for how to configure this), they are stored in this table. The actual mapping of start times to jobs is stored in the `DBSolutionJob` table, which references into the `DBSolution` table. Fields:

* `res`: Foreign Key into the `DBResult` table. Indicates which result this solution belongs to.

### DBSolutionJob

Stores the assignment of start times to jobs for the `DBSolution`s. Fields:

* `sol`: Foreign Key into the `DBSolution` table.
* `job_id`: Job ID of the job that is being assigned a start time.
* `start_time`: The start time assigned to the job with ID `job_id`.

### DBIntermediate

Allows solvers to report intermediate results during optimization. The solver must be configured to report intermediate results. Fields:

* `res` Foreign Key into `DBResult`: The (final) result that this intermediate result belongs to.
* `time` The time (in seconds) after which this intermediate result was obtained.
* `iterations` The number of iterations after which this intermediate result was obtained. The exact meaning of "iteration" depends on the solver used.
* `costs` The costs achieved with this intermediate result.
* `bound` The lower bound achieved with this intermediate result.
* `solution` A solution for this intermediate result, if available.

### DBExtendedMeasure

Some solvers allow to record extended measures during optimization, e.g., memory consumption etc. The solver must be configured to report extended measures. Fields:

* `res`: Foreign Key into `DBResult`. The result that this extended measure belongs to.
* `key`: A string key of the measure. This is defined by the solvers.
* `v_int`: An integer value, if this measure is of integer type.
* `v_double`: A float value, if this measure is of floating-point type.
* `iteration`: The number of iterations after which this measure was taken. The exact meaning of "iteration" depends on the solver used.
* `time` The time (in seconds) after which this measure was taken.

### DBError

If a solver terminates with an error that can be caught by the framework, the error is logged in `DBError`. Every row is one error. Fields:

* `timestamp` The UNIX timestamp at which the error occurred
* `run` The run ID that the solver was running with.
* `instance` The ID of the instance that the solver was working on.
* `algorithm` The ID of the solver.
* `seed` The seed that the solver was run with.
* `fault_code` A numeric code representing the subtype of the error.
* `error_id` A numeric code representing the error.

Examples
--------

Say you want to see the results for the instance with ID 'c3fa8fa6-7827-465c-9789-0e8451b4e97c' from set PS-Uniform. This is what you do:

```
sqlite3 set_PS-Uniform.db

sqlite> select score, algorithm, optimal, lower_bound, elapsed from DBResult where instance = 'c3fa8fa6-7827-465c-9789-0e8451b4e97c';
593.963947565193|EarlyScheduler v1|0||4.738e-05
520.850476382213|ILPBase v2.3 (Gurobi)|1|520.842921733248|1652.891681388
```

What we see here is:
* The score (i.e., peak demand) without optimization (i.e., "optimized" by the EarlyScheduler) was ~593.96 (the row with "EarlyScheduler v1"), the peak demand after optimization with the ILP was ~520.85.
* Optimization ran to optimality (note the `1` in the `optimal` field) within ~1653 seconds.
* Since we were able to optimize to optimality, the `lower_bound` field has the same value as the `score` field.
