#!/usr/bin/env python

#------------------------------------------------------------------------------
# Bash script to build cpp-ethereum within TravisCI.
#
# The documentation for cpp-ethereum is hosted at http://cpp-ethereum.org
#
# ------------------------------------------------------------------------------
# This file is part of cpp-ethereum.
#
# cpp-ethereum is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# cpp-ethereum is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>
#
# (c) 2016 cpp-ethereum contributors.
#------------------------------------------------------------------------------
#
#    Copyright (C) 2018-2019 SKALE Labs
#
#    This file is part of skale-consensus.
#
#    skale-consensus is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, eithe    r version 3 of the License, or
#    (at your option) any later version.
#
#    skale-consensus is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.
#
#    @file  build.py
#    @author Stan Kladko
#    @date 2018
#

import sys
import os
import subprocess


def run(_command):
    print(">" +_command)
    subprocess.check_call(_command, shell = True)

assert len(sys.argv) == 3

os.chdir("..")

print("Starting build")

print("Current directory is" + os.getcwd())

print("Got TRAVIS_BUILD_TYPE=" + sys.argv[1])
print("Got TRAVIS_BUILD_DIR=" + sys.argv[2])


run("wget https://cmake.org/files/v3.10/cmake-3.10.3-Linux-x86_64.sh")
run("chmod +x cmake-3.10.3-Linux-x86_64.sh");
run("./cmake-3.10.3-Linux-x86_64.sh --skip-license")
run("ln -s ./cmake-3.10.3-Linux-x86_64/bin/cmake /usr/bin/cmake");

cmakeExecutable = subprocess.check_output(["which", "cmake"])

print("Running cmake: " + cmakeExecutable)


run ("ccache -M 20G")
run("./libBLS/deps/build.sh PARALLEL_COUNT=j$(nproc)")
run("cmake . -Bbuild -DCMAKE_BUILD_TYPE=" +  sys.argv[1] +
                        " -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0")

run("cmake --build build -- -j4")

buildDirName = sys.argv[2] + '/build'

print("Build dir:" + buildDirName)


run("ls " + buildDirName)

assert  os.path.isfile(sys.argv[2] + '/build/consensust')
assert  os.path.isfile(sys.argv[2] + '/build/consensusd')

print("Build successfull.")



