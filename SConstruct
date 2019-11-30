#!/usr/bin/env python

# -----------------------------------------------------------------------------
# The copyright notice below is for the SCons build scripts included with this
# software, which were initially developed by Michael Park
# (see https://github.com/mpark/bob) and customized at if(we).  Many thanks to
# Michael for this useful contribution.
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# The MIT License (MIT)
# Copyright (c) 2014 Michael Park
# Copyright (c) 2014 if(we)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# -----------------------------------------------------------------------------

import filecmp
import os
import shutil
import stat
import subprocess
import sys


def is_path_prefix(path1, path2):
    if path1 is None:
        return True

    c1 = path1.split('/')

    if c1[-1] == '':
        # Eliminate empty item after trailing '/'.
        c1 = c1[:-1]

    c2 = path2.split('/')

    if len(c1) > len(c2):
        return False

    for i in range(len(c1)):
        if c2[i] != c1[i]:
            return False

    return True


# Options.
AddOption('--test',
          action='store_true',
          help='Compile and execute unit tests.')
AddOption('--release',
          action='store_true',
          help='Build release targets (debug targets are built by default).')
AddOption('--import_path',
          action='store_true',
          help='Import PATH environment variable into build environment.')
AddOption('--asan',
          action='store',
          help='Enable or disable address sanitizer in debug build ' + \
               '(ignored for production build).', dest='asan',
          choices=('yes', 'no'), default='yes')

# Our mode.
if GetOption('release') is None:
    mode = 'debug'
else:
    mode = 'release'

# Paths.
root = Dir('#')
src = root.Dir('src')
third_party = src.Dir('third_party')
tclap = third_party.Dir('tclap-1.2.0').Dir('include')
gtestbasedir = third_party.Dir('googletest')
gtestincdir = gtestbasedir.Dir('googletest-release-1.7.0').Dir('include')
out_base = root.Dir('out')
out = out_base.Dir(mode)

if GetOption('clean'):
    try:
        shutil.rmtree(out_base.get_path())
    except OSError:
        pass

    sys.exit(0)

if not is_path_prefix(src.get_abspath(), GetLaunchDir()):
    sys.stderr.write('You must build from within the "src" part of the ' + \
            'tree, unless you are doing a "clean" operation (-c option).\n')
    sys.exit(1)

if not BUILD_TARGETS:
    print('No build target specified')
    sys.exit(0)


# Look for Dory's generated version file.  If the file exists and its version
# string is incorrect, delete it.  Deleting the file will cause the build to
# regenerate it with the correct version string.
def check_version_file(ver_file):
    check_ver = src.Dir('dory').Dir('scripts').File('gen_version') \
            .get_abspath()

    try:
        subprocess.check_call([check_ver, '-c', ver_file]);
    except subprocess.CalledProcessError:
        sys.stderr.write(
            'Failed to execute script that checks generated version file ' + \
            ver_file +'\n')
        sys.exit(1)


check_version_file(out.Dir('dory').File('build_id.c').get_abspath())
check_version_file(out.Dir('dory').Dir('client').File('build_id.c').
        get_abspath())

xerces_lib_deps = ['xerces-c']

ext_lib_deps = [
    [r'^dory/dory\.test$', xerces_lib_deps],
    [r'^dory/unix_dg_input_agent\.test$', xerces_lib_deps],
    [r'^dory/stream_client_handler\.test$', xerces_lib_deps],
    [r'^dory/conf/conf\.test$', xerces_lib_deps],
    [r'^xml/.*', xerces_lib_deps],
    [r'^dory/dory$', xerces_lib_deps]
]

# Environment.
prog_libs = {'pthread', 'dl', 'rt', 'z'}
env = Environment(CFLAGS=['-Wwrite-strings'],
        CCFLAGS=['-Wall', '-Wextra', '-Werror', '-Wformat=2', '-Winit-self',
                '-Wunused-parameter', '-Wshadow', '-Wpointer-arith',
                '-Wcast-align', '-Wlogical-op', '-Wno-nonnull-compare'],
        CPPDEFINES=[('SRC_ROOT', '\'"' + src.abspath + '"\'')],
        CPPPATH=[src, tclap, gtestincdir],
        CXXFLAGS=['-std=c++11', '-Wold-style-cast', '-Wno-noexcept-type'],
        DEP_SUFFIXES=['.cc', '.cpp', '.c', '.cxx', '.c++', '.C'],
        PROG_LIBS=[lib for lib in prog_libs],
        TESTSUFFIX='.test',
        EXT_LIB_DEPS=ext_lib_deps)

if GetOption('import_path'):
    env['ENV']['PATH'] = os.environ['PATH']


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
    # Enabling link-time optimizations breaks the build on Ubuntu 15, so for
    # now we avoid enabling them.  What a chore it is to get stuff to build on
    # multiple platforms.  :-(

    env.AppendUnique(CCFLAGS=['-O2', '-DNDEBUG', '-Wno-unused',
                              '-Wno-unused-parameter', '-fvisibility=hidden'])

    # Unfortunately this is needed to prevent spurious build errors on Ubuntu
    # 14.  :-(
    env.AppendUnique(CPPFLAGS=['-U_FORTIFY_SOURCE'])

    env.AppendUnique(LINKFLAGS=['-rdynamic'])


# Append 'mode' specific environment variables.
{'debug'  : lambda: set_debug_options(),
 'release': lambda: set_release_options()
}[mode]()

# Export variables.
Export(['env', 'src', 'out'])

# Run the SConscript file.
env.SConscript(src.File('SConscript'), variant_dir=out, duplicate=False)
