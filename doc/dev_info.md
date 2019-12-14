## Modifying Dory's Implementation

Before making code changes, it is helpful to become familiar with Dory's build
system, which is based on [SCons](http://www.scons.org/).

### Build System

Files [SConstruct](../SConstruct) and [src/SConscript](../src/SConscript)
contain the build configuration.  As shown
[here](build_install.md#building-dory-directly), to build something, first
source the file [bash_defs](../bash_defs) in the root of Dory's Git
repository.  Then `cd` into the `src` directory or any directory beneath `src`
and use the `build` command to build a particular target.  In general, to build
an executable you simply specify its name when invoking the `build` command.
For instance:

```
source bash_defs
cd src/dory
build dory  # builds dory executable
build client/to_dory  # builds command line client
build msg.o  # compile source file msg.cc
build conf/conf.o  # compile source file conf/conf.cc
```

If you are building on CentOS/RHEL 7, remember to build within a shell started
by `scl enable devtoolset-8 bash`, as detailed [here](centos_7_env.md).

All build results are in the `out` directory (relative to the root of the Git
repository).  For instance, the built `dory` executable will be
`out/debug/dory/dory`.  To build a release version of a target, use the
`--release` option with the `build` command.  For instance,
`build --release dory` will build a release version of `dory`, which will be
`out/release/dory/dory`.

Source files ending in `.test.cc` are unit tests, which can be executed as
standalone executables.  For instance, in the above example if we typed
`build unix_dg_input_agent.test`, this would build the unit test for Dory's
UNIX datagram input agent, which would then appear as executable file
`out/debug/dory/unix_dg_input_agent.test`.  If you type
`build --test unix_dg_input_agent.test`, that will build the unit test and then
immediately execute it.

If you type `build -c`, that will remove all build artifacts by deleting the
`out` directory.  For `make` users, this is the equivalent of `make clean`.
Alternatively, you can just type `rm -fr out` from the root of the Git
repository.  If you wish to change any compiler or linker flags, look for the
section of code in the SConstruct file that looks roughly like this:

```Python
xerces_lib_deps = ['xerces-c']

ext_lib_deps = [
    [r'^dory/dory\.test$', xerces_lib_deps],
    [r'^dory/unix_dg_input_agent\.test$', xerces_lib_deps],
    [r'^dory/stream_client_handler\.test$', xerces_lib_deps],
    [r'^dory/conf/conf\.test$', xerces_lib_deps],
    [r'^xml/.*', xerces_lib_deps],
    [r'^dory/dory$', xerces_lib_deps]
]

env = Environment()

# As documented here:
#
#     https://www.softwarecollections.org/en/scls/rhscl/devtoolset-8/
#
# gcc 8 is enabled on CentOS 7 by building in an environment set up by the
# following command:
#
#     scl enable devtoolset-8 bash
#
# See if we are executing in this kind of environment.  If so, we must inject
# our environment variables into the build environment so gcc 8 is used.
if 'X_SCLS' in os.environ and \
        'devtoolset-8' in os.environ['X_SCLS'].split(' '):
    env.Append(ENV=os.environ)

prog_libs = {'pthread', 'dl', 'rt', 'z'}
env.Append(CFLAGS=['-Wwrite-strings'],
        CCFLAGS=['-Wall', '-Wextra', '-Werror', '-Wformat=2', '-Winit-self',
                '-Wunused-parameter', '-Wshadow', '-Wpointer-arith',
                '-Wcast-align', '-Wlogical-op', '-Wno-nonnull-compare'],
        CPPDEFINES=[('SRC_ROOT', '\'"' + src.abspath + '"\'')],
        CPPPATH=[src, tclap, gtestincdir],
        CXXFLAGS=['-std=c++17', '-Wold-style-cast', '-Wno-noexcept-type'],
        DEP_SUFFIXES=['.cc', '.cpp', '.c', '.cxx', '.c++', '.C'],
        PROG_LIBS=[lib for lib in prog_libs],
        TESTSUFFIX='.test',
        EXT_LIB_DEPS=ext_lib_deps)


def set_debug_options():
    # Note: If you specify -fsanitize=address, you must also specify
    # -fno-omit-frame-pointer and be sure libasan is installed (RPM package
    # libasan on RHEL, Fedora, and CentOS).
    env.AppendUnique(CCFLAGS=['-g3', '-ggdb', '-fno-omit-frame-pointer',
                              '-fvisibility=hidden'])
    env.AppendUnique(CXXFLAGS=['-D_GLIBCXX_DEBUG',
                               '-D_GLIBCXX_DEBUG_PEDANTIC',
                               '-DTRACK_FILE_DESCRIPTORS'])
    env.AppendUnique(LINKFLAGS=['-rdynamic'])

    if GetOption('asan') == 'yes':
        env.AppendUnique(CCFLAGS=['-fsanitize=address'])
        env.AppendUnique(LINKFLAGS=['-fsanitize=address'])


def set_release_options():
    # TODO: Enable link-time optimizations.
    env.AppendUnique(CCFLAGS=['-O2', '-DNDEBUG', '-Wno-unused',
                              '-Wno-unused-parameter', '-fvisibility=hidden'])
    env.AppendUnique(LINKFLAGS=['-rdynamic'])
```

Note that SCons build files are actually Python scripts, so you can add
arbitrary Python code.  Adding, removing or renaming source files does not
require any changes to the build scripts, since they are written to figure out
the dependencies on their own.  If you want to build all targets (or a
substantial subset of all targets) with a single command, you can execute the
[build_all](../build_all) script in the root of the Git repository.  For
instance, `build_all run_tests` will build and run all unit tests.  Type
`build_all --help` for a full description of the command line options.
Eventually it would be nice to eliminate the `build_all` script and integrate
its functionality directly into the SCons configuration.

### Debug Builds

As shown above, if you use the `build` command to build an individual binary,
it will create a debug version by default if `--release` is not specified.
Likewise, if you do not specify `--mode=release` when running the `build_all`
script, debug versions of binaries will be built by default.  As documented
[here](build_install.md#building-an-rpm-package), the `pkg` command may be used
for building an RPM package.  By default, `pkg` builds a release version of
Dory.  To build a debug version, specify `--debug`.

GCC provides support for
[AddressSanitizer](http://code.google.com/p/address-sanitizer/), a useful
debugging tool.  This is enabled by default in debug builds.  When running Dory
with the address sanitizer, you may notice that it uses a large amount of
virtual memory (often multiple terabytes).  This is expected behavior.  After
running Dory for a while with the address sanitizer, it may exit with the
following error message:

```
ERROR: Failed to mmap
```

If this causes problems, you can build Dory with the address sanitizer
disabled.  When building Dory directly, as described
[here](build_install.md#building-dory-directly), you can disable the address
sanitizer as follows:

```
build --asan=no dory
```

Likewise, you can invoke `build_all` as follows:

```
./build_all --asan=no
```

When building an RPM package (as described
[here](build_install.md#building-an-rpm-package)), a debug version with the
address sanitizer disabled may be created as follows:

```
./pkg --debug --asan no rpm
```

or

```
./pkg --debug --asan no rpm_noconfig
```

Another thing to keep in mind about the address sanitizer's behavior is that it
causes `operator new` to return `nullptr` on allocation failure in cases where
the C++ standard dictates that `std::bad_alloc` should be thrown.  This can
cause out of memory conditions to result in segmentation faults due to null
pointer dereferences.  See
[this issue](https://github.com/google/sanitizers/issues/295) for details.

Regardless of how you build dory (via the `build` command, the `build_all`
script, or the `pkg` script), the address sanitizer is *always* disabled in
release builds, regardless of any command line options that disable or enable
the address sanitizer.

The GNU C++ library provides a
[debug mode](https://gcc.gnu.org/onlinedocs/libstdc++/manual/debug_mode.html)
which implements various assertion checks for STL containers.  Dory makes use
of this in its debug build. A word of caution is therefore necessary. Suppose
you have the following piece of code:

```C++
/* Do something interesting to an array of int values.  'begin' points to the
   beginning of the array and 'end' points one position past the last element.
   'begin' and 'end' will be equal in the case of an empty input. */
void DoSomethingToIntArray(int *begin, int *end) {
  assert(begin || (end == begin));
  assert(end >= begin);
  // do something interesting ...
}

void foo(std::vector<int> &v) {
  DoSomethingToIntArray(&v[0], &v[v.size()]);
}
```

The above code is totally legitimate C++.  However, the expression
`&v[v.size()]` will cause an out of range vector index to be reported when
running with debug mode enabled.  In fact, the expression `&v[0]` is enough to
cause an error to be reported in the case where `v` is empty.  Therefore the
code needs to be written a bit differently to avoid spurious errors in debug
builds. For instance, one might instead implement foo() like this:

```C++
void foo(std::vector<int> &v) {
  if (!v.empty()) {
    DoSomethingToIntArray(&v[0], &v[0] + v.size());
  }
}
```

Although this is a bit less elegant than the previous implementation, the
benefits of tools such as debug mode can be great when tracking down problems.
Therefore please avoid code such as the first version of `foo()` when making
changes to Dory.

### Contributing Code

Information on contributing to Dory is provided [here](../CONTRIBUTING.md).

-----

dev_info.md:
Copyright 2019 Dave Peterson (dave@dspeterson.com)
Copyright 2014 if(we), Inc.

dev_info.md is licensed under a Creative Commons Attribution-ShareAlike 4.0
International License.

You should have received a copy of the license along with this work. If not,
see <http://creativecommons.org/licenses/by-sa/4.0/>.
