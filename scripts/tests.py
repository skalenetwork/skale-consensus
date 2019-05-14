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
import sys



def fullConsensusTest(_test, _consensustExecutive):
    testDir = root + "/test/" + _test
    os.chdir(testDir)
    os.system("rm  -rf " + testDir + "/core")
    os.system("rm  -rf /tmp/*.db")
    subprocess.call(_consensustExecutive)


root = sys.argv[1]


def getConsensustExecutive():
    consensustExecutive = root + '/cmake-build-debug/consensust'
    if not os.path.isfile(consensustExecutive):
        subprocess.call("ls " + root)
        consensustExecutive = root + "/consensust"
        print consensustExecutive
        assert os.path.isfile(consensustExecutive)
    return consensustExecutive


consensustExecutive = getConsensustExecutive()

fullConsensusTest("onenode", consensustExecutive)
fullConsensusTest("twonodes", consensustExecutive)
fullConsensusTest("fournodes", consensustExecutive)
fullConsensusTest("sixteennodes", consensustExecutive)




