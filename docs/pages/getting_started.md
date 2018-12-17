@page gettingstarted Getting Started

This is the TCPSPSuite, a software suite for optimizing various flavours of the Time Constrained Project Scheduling Problem. Refer to @ref instancefileformat for what features instances to be optimized may include.

Requirements
------------

TCPSPSuite should work on any moderately recent GNU/Linux system. All the libraries listed here must be installed including development headers. On many Linux distributions, the development package names end in `-dev`.

* Boost >= 1.61
* A recent C++ compiler: GCC >= 7 (note there is a [bug in GCC == 8.0](https://github.com/ldionne/dyno/issues/55) that causes problems) or clang >= 4 will do.
* SQLite3
* ODB == 2.4.0, including the SQLite runtime (this one: https://www.codesynthesis.com/products/odb/) - **Note**: You most probably need exactly version 2.4.0, since we need to patch its header files to work with C++17. If you try it with a different version of ODB and it works, please let us know! Also see the [section on troubleshooting ODB](#odb-troubleshooting) below.
* For using the ILP solver: Gurobi >= 7.0 ([see below for installation instructions](#gurobi-troubleshooting)) or a recent CPLEX version
* CMake >= 3.0

### <a name="gurobi-troubleshooting"></a> Installing Gurobi

Installing Gurobi can be tricky sometimes. Here are some pointers:

* **Most importantly**: Most modern compilers can't work with the shipped `libgurobi_c++.a`. This yields compilation errors like `undefined reference to <something about gurobi>` in the linking step. To fix this problem, you need to copy the file ``/path/to/gurobi/lib/libgurobi_g++<highest version available>.a` over the file `/path/to/gurobi/lib/libgurobi_c++.a`. The actually available versions of that file differ depending on your Gurobi version. Just use the most recent version. 
* Set the `GUROBI_HOME` environment variable to the path where you installed Gurobi to. It should point to the `linux64` folder, which contains e.g. `lib`, `setup.py`, etc.
* Set the `GRB_LICENSE_FILE` environment variable to the path where you put the license file downloaded via `grbgetkey` (see the Gurobi documentation)
* The file `FindGUROBI.cmake` in the `cmake/modules` subdirectory for technical reasons has a hard-coded list of Gurobi versions that are detected. Currently, the most recent detected version is Gurobi 8.0. If you have a more recent version of Gurobi, you might need to add it to the list of detected Gurobi versions in that file.

### <a name="odb-troubleshooting"></a> Troubleshooting ODB

ODB in its current version (2.4.0) is not C++17 compliant, since it uses the removed `throw` declaration. Otherwise, the code is C++17 compliant. Therefore, our main `CMakeLists.txt` currently includes these steps:

1. Copy the system-installed ODB headers into the subdirectory `src/contrib/odb_headers`
2. Replace all occurrences of `throw ()` with `noexcept`.
3. Remove all occurrences of `throw (std::bad_alloc)` from the files via a simple regular expression
4. Add `src/contrib/odb_headers` to the include path.

If this automatic process does not work for you, a way of circumventing it would be:

1. Prepare C++17 compliant ODB headers yourself. The two replacements resp. removals listed above should be enough.
2. Copy the headers somewhere in the include path. If necessary, [modify the include path](https://cmake.org/cmake/help/latest/variable/CMAKE_INCLUDE_PATH.html) as required.


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

If you run it without any arguments, it will print a usage help. To select which solvers should be run on your instances, you need to create a @ref solver_configurations "solver configuration file". An example is provided in the example folder.

Getting Help
------------

If you think you encountered a bug, please report it in [the bug tracker](https://github.com/kit-algo/TCPSPSuite/issues). You can also [contact us via mail](mailto:lukas.barth@kit.edu).
