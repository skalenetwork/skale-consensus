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

os.chdir("..")

print("Starting build")

print("Current directory is" + os.getcwd())

print("Got TRAVIS_BUILD_TYPE=" + sys.argv[1])
print("Got TRAVIS_BUILD_DIR=" + sys.argv[2])


cmakeExecutable = subprocess.check_output(["which", "cmake"])

print("Running cmake: " + cmakeExecutable)

subprocess.call(["cmake", ".",  "-DCMAKE_BUILD_TYPE=" +  sys.argv[1], "-DCOVERAGE=ON", "-DMICROPROFILE_ENABLED=0"])


subprocess.call(["/usr/bin/make", "-j4"])

assert  os.path.isdir(sys.argv[2])
assert  os.path.isfile(sys.argv[2] + '/cmake-build-' + sys.argv[1].lower() +  '/consensust')
assert  os.path.isfile(sys.argv[2] + '/cmake-build-' + sys.argv[1].lower() + '/consensusd')



