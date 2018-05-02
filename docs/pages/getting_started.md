@page gettingstarted Getting Started

This is the TCPSPSuite, a software suite for optimizing various flavours of the Time Constrained Project Scheduling Problem. Refer to @ref instance_file_format for what features instances to be optimized may include.

Requirements
------------

All the libraries listed here must be installed including development headers. On many Linux distributions, the development package names end in `-dev`.

* Boost >= 1.61
* A recent C++ compiler: GCC >= 7 or cmake >= 4 will do.
* SQLite3
* ODB == 2.4.0, including the SQLite runtime (this one: https://www.codesynthesis.com/products/odb/) - **Note**: You most probably need exactly version 2.4.0, since we need to patch its header files to work with C++17. If you try it with a different version of ODB and it works, please let us know!
* For using the ILP solver: Gurobi >= 7.0 or a recent CPLEX version

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
At this point you should be notified if any required library (see Requirements above) is not found. You either need to install the respective library or make sure that CMake is able to find it. Please note that at the bottom of the output, CMake tells you which optional packages could be found resp. not found, and which solvers need which optional packages. If an optional package is not being found, no solvers that need it will be built.

- Build TCPSPSuite:
```
make
```
This will take a couple of minutes.

- Afterwards, the binary can be found in `build/src/tcpspsute`.

Running
-------

If you run it without any arguments, it will print a usage help.

Getting Help
------------

If you think you encountered a bug, please report it in [the bug tracker](https://github.com/kit-algo/TCPSPSuite/issues). You can also [contact us via mail](mailto:lukas.barth@kit.edu).
