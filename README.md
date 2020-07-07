TCPSPSuite
==========

This is the TCPSPSuite, a software suite for optimizing various flavours of the Time Constrained Project Scheduling Problem. Refer to [the documentation](https://kit-algo.github.io/TCPSPSuite/) for details.

Requirements
------------

TCPSPSuite should work on any moderately recent GNU/Linux system. All the libraries listed here must be installed including development headers. On many Linux distributions, the development package names end in `-dev`.

* Boost >= 1.61
* A recent C++ compiler: GCC >= 7 or clang >= 4 will do.
* SQLite3
* ODB == 2.4.0, including the SQLite runtime (this one: https://www.codesynthesis.com/products/odb/) - **Note**: You most probably need exactly version 2.4.0, since we need to patch its header files to work with C++17. If you try it with a different version of ODB and it works, please let us know!
* For using the ILP solver: Gurobi >= 7.0 or a recent CPLEX version
* CMake >= 3.0

Installing some of these requirements, especially Gurobi, can be tricky. Please refer to [the getting started page of the documentation](https://kit-algo.github.io/TCPSPSuite/gettingstarted.html)

Getting the Source
------------------

The source is available from github. Just clone the repository and
fetch its submodules. Example:

```
git clone https://github.com/kit-algo/TCPSPSuite.git ./TCPSPSuite
cd TCPSPSuite
git submodule init
git submodule update --recursive
```

Building
--------

- For the following, we assume that you checked out the repository (see above) to `~/src/tcpsp/TCPSPSuite`.


- Create a build repository outside of the code repository folder. Example:
```
cd ~/src/tcpsp
mkdir build
```
- Inside your `build` directory, use CMake to configure the build process:
```
cd build
cmake ../TCPSPSuite
```
At this point you should be notified if any required library (see Requirements above) is not found. You either need to install the respective library or make sure that CMake is able to find it. 

**Important**: Please note that at the bottom of the output, CMake tells you which optional packages could be found resp. not found, and which solvers need which optional packages. If an optional package is not being found, no solvers that need it will be built. The package will build fine without any solvers at all - however, you won't be able to do any optimization!

- Build TCPSPSuite:
```
make
```
This will take a couple of minutes.

- Afterwards, the binary can be found in `build/src/tcpspsute`.

Running
-------

If you run it without any arguments, it will print a usage help. To select which solvers should be run on your instances, you need to create a [solver configuration file](https://kit-algo.github.io/TCPSPSuite/solver_configurations.html). An example is provided in the example folder.

TCPSPSuite needs at least two arguments to be run: A (sqlite3) file in which to store the results and an instance file or a directory in which to look for instance files. A simple invocation could therefore be:

```
tcpspsuite -s /path/to/storage.sqlite3 -f /path/to/my/instance.json
```

Or, if you want to solve all instances in a directory:

```
tcpspsuite -s /path/to/storage.sqlite3 -d /path/to/my/instance/directory
```

Let's add a solver configuration file:

```
tcpspsuite -s /path/to/storage.sqlite3 -d /path/to/my/instance/directory -c /path/to/the/solver/config.json
```


Getting Help
------------

If you think you encountered a bug, please report it in [the bug tracker](https://github.com/kit-algo/TCPSPSuite/issues). You can also [contact us via mail](mailto:lukas.barth@kit.edu).
