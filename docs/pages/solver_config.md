@page solver_configurations Solver Configurations

TCPSPSuite must be configured to run at least one of several @ref solvers on the instances that have been selected. The solver configuration file is where this configuration is specified.

The outermost object of any solver config file contains a single member named "solvers", which in turn contains an array. Every object in this array corresponds to one solver configuration that you ask TCPSPSuite to run. Every solver configuration contains three members:

* **name**: The name of this configuration. Will be written to the database for easier identification of the results that were computed with this configuration.
* **time_limit**: The number of seconds after which the solver should be interrupted.
* **regex**: A regular expression that is matched against the IDs of the @ref solvers. Every solver the ID of which matches this regular expression will be run on all instances with this configuration.
* **config**: An object that specifies key-value mappings which can be used to fine-tune the individual solvers. What options are available and what they mean is dependent on the solver used. See the @ref solvers page for details.

Note that you can specify multiple solver configurations that match the same solvers. This allows you to run the same solver with different configurations on all instances.

Example
-------

Let's say you want to run the `ILPBase` solver (see @ref solvers) for 2 minutes using Gurobi as MIP solver, and you want to experiment what the influence of setting the `focus` parameter to `1` or `2` is. This would be your solver config file:

~~~~~~~~~~~~~{.md}
{
  "solvers": [
    {
      "regex": "ILPBase.*Gurobi.*",
      "time_limit": "120",
      "name": "focus_1",
      "config": {
        "focus": 1
      }
    },
    {
      "regex": "ILPBase.*Gurobi.*",
      "time_limit": "120",
      "name": "focus_2",
      "config": {
        "focus": 2
      }
    }
  ]
}
~~~~~~~~~~~~~
