#!/usr/bin/env python
#
#    Copyright (C) 2018-2019 SKALE Labs
#
#    This file is part of skale-consensus.
#
#    skale-consensus is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
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
#    @file  tests.py
#    @author Stan Kladko
#    @date 2019


import os
import subprocess
import sys;


root = sys.argv[1]

os.chdir(root + "/test/fournodes")
os.system("rm  -rf " + root + "/core")
os.system("rm  -rf /tmp/*.db")
subprocess.call(["ls", root])
subprocess.call(["ls", root + "/cmake-build-debug"])
consensust = root + '/cmake-build-debug/consensust'
if not os.path.isfile(consensust):
    consensust = root + "/consensust"
    assert os.path.isfile(consensust)

subprocess.call(consensust)


