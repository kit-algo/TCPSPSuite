@page instancefileformat Instance File Format

A TCPSPSuite instance is described by a JSON file. The two most important aspects of an instance are its **resources** and its **jobs**. Jobs are the building blocks of an instance. The TCPSPSuite will schedule each defined job exactly once. The output (i.e., the optimized solution of an instance) consists of an assignment of a start time to every job.

In a TCPSP instance, each job is associated with a certain quantity of one or more resources. The job requires these quantities while it is being executed. The **resource** specification in an instance file allows to specify costs for the consumption of resources.

Additionally, an instance has an **id** field uniquely identifying this instance, and may have an **additional** field containing arbitrary key-value mappings. These mappings are not used by TCPSPSuite but may be used to tag instances, for example supply information on how the instance was generated.

Full example
------------
Before listing the possible objects and their attributes, we provide a full (although somewhat minimal) example. This instance consists of three jobs and one resource. Job `0` must be run for 15 time steps between time step 0 and 41 and takes 2.5 units of resource `0` during that time. Job `1` must be run for 2 time steps between time steps 0 and 48, consuming 5 units of resource `0`, and job `2` must be run for 5 time steps between time steps 0 and 56, requiring 10 units of resource `0`. Additionally, job `2` can only be started as soon as job `1` was started, no earlier.

The costs of a solution are the maximum amount of resource `0` that is used times 10.

This is the corresponding instance file:

```json
{
  "jobs": [
    {
      "id": 0,
      "usages": {
        "0": 2.5
      },
      "successors": {},
      "deadline": 41,
      "release": 0,
      "duration": 15
    },
    {
      "id": 1,
      "usages": {
        "0": 5
      },
      "successors": {
        "2": {
          "drain_factor": 0,
          "max_recharge": 0,
          "lag": 0
        }
      },
	  "additional": {
	    "comment": "This is job nr. 1"
	  },
      "deadline": 48,
      "release": 0,
      "duration": 2
    },
    {
      "id": 2,
      "usages": {
        "0": 10
      },
      "successors": {},
      "deadline": 56,
      "release": 0,
      "duration": 5
    }
  ],
  "id": "minimal test instance",
  "additional": {
    "instance_comment": "This is an example instance"
  },
  "resources": [
    {
      "id": 0,
      "investment_costs": [
        [
          10,
          1
        ]
      ],
      "overshoot_costs": [],
      "free_amount": 0
    }
  ]
}
```

Jobs
----
Each job has these attributes:

* **id** (Required): An integer ID identifying this job. IDs must start at 0, and are required to be consecutive and unique (i.e., jobs must have IDs 0, 1, 2, 3, …)
* **successors** (Required): An (optionally empty) map of job IDs to lag attributes. Every entry in this map describes one job the execution of which depends on this job. The lag attributes are:
  * **lag** (Required): A number of time steps. The dependent job can start no earlier than *lag* time steps after the start of the current job. For example, take this excerpt:
```json
{ …
  "jobs": [
    {"id": 42,
     "successors": {
       "23": {"lag": 100, …}
      },
     …
     }
] }
```
  This would indicate that job `23` can start at the earliest 100 time steps after job `42` was started.
  * **drain_factor**: A floating number, describing how much the duration of the dependent job is extended for every time step that the dependent job is started later than possible with regard to the current job. For example:
```json
{ …
  "jobs": [
    {"id": 42,
     "successors": {
       "23": {"lag": 100, "drain_factor": 0.5, …}
      },
     …
     }
] }
```
  Now suppose that job `42` is started at time step 0, and job `23` is started at time step 150, even though the time lag from job `42` would have allowed to start `23` at time step 100 already. Thus, `23` was started 50 time steps later than possible with regard to `42`. Thus, the duration of job `23` is extended by `50 * 0.5 = 25` time steps.
  * **max_recharge**: A number, setting a maximum for the duration extension defined above. Consider this example:  
```json
{ …
  "jobs": [
    {"id": 42,
     "successors": {
       "23": {"lag": 100, "drain_factor": 0.5, "max_recharge": 10}
      },
     …
     }
] }
```
  Now, the duration extension of job `23` would be computed as above and amount to 25 time steps. However, the `max_recharge` attribute limits the duration extension to 10 time steps, and therefore job `23`'s duration is only extended by 10 time steps.

* **usages** (Required): A map from resource IDs to a number. Every resource defined in the instance must be present. The mapped-to number indicates how much of the resprective resource this job requires during execution. Example:
```json
{
  "resources": [
    {"id": 0, …},
    {"id": 1, …}
  ],
  "jobs": [
    {"id": 42,
     "usages": {
       "0": 1,
       "1": 3.1415
     },
     …
     }
],
…
 }
```
  This defines two resources with IDs `0` and `1` (see below for details on resources) and states that job `42` requires one unit of resource `0` and 3.1415 units of resource `1` while executing.

* **release** (Required): The time step that the job can be started at the earliest (if no window extensions are used, see below).
* **deadline** (Required): The time step at which the job must be finished at the latest (if no window extensions are used, see below).
* **duration** (Required): The number of time steps that the job must execute consecutively. This time can be extended, e.g. by `drain_factor`s.
* **additional** (Optional): A dictionary with arbitrary key-value mappings. May be used to tag jobs, etc. Solvers ignore the values set here.

Resources
---------
Each resource has these attributes:
* **id** (Required): An integer ID identifying this resource. IDs must start at 0, and are required to be consecutive and unique (i.e., resources must have IDs 0, 1, 2, 3, …)
* **availability**: Defines a stepwise function indicating how many units of this resource are available without costs at which time step. The stepwise function is given as a list of pairs, where the first item of the pair defines a time step and the second item defines the amount available from this time step on, until the next point in the stepwise function. The time steps must be strictly increasing and start at 0. Example:
```json
{
  "resources": [
    {"id": 0, "availability": [ [0,0], [1,10], [5,2.5], [10,0] ] },
  ],
…
 }
```
This defines a resource `0` which has availability of 10 units in the interval `[1,5)`, availability of 2.5 units in the interval `[5,10)`, and zero units available everywhere else. If `availability` is not given, zero units are available.

* **investment_costs**: Defines the function of the costs for the *maximum* usage of this resource *not covered by availability*. Costs are defined as a polynomial. The polynomial is gives as a list of pairs, where the first item of each pair defines a coefficient and the second item of the pair defines an exponent of a term. Example:
```json
{
  "resources": [
    {"id": 0, "investment_costs": [ [3,1], [5,2] ] },
  ],
…
 }
```
This defines a resource `0` with costs `3 * x^1 + 5 * x^2`, where `x` is the maximum amount that the usage of resource `0` exceeds its availability.
* **overshoot_costs**: Defines the function of the costs for the usage of this resource *not covered by availability* at every time step. Costs are defined as for `investment_costs`.
