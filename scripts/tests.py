#!/usr/bin/env python
#
#    Copyright (C) 2018-2019 SKALE Labs
#
#    This file is part of skale-consensus.
#
#   skale-consensus is free software: you can redistribute it and/or modify
#   it under the terms of the GNU Affero General Public License as published
#   by the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   skale-consensus is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.
#
#    @file  tests.py
#    @author Stan Kladko
#    @date 2019


import os
import subprocess
import sys



def fullConsensusTest(_test, _consensustExecutive, _testType):
    testDir = root + "/test/" + _test
    os.chdir(testDir)
    os.system("bash -c rm  -rf " + testDir + "/core")
    os.system("bash -c rm  -rf /tmp/*.db")
    subprocess.call([_consensustExecutive, _testType])


def getConsensustExecutive():
    consensustExecutive = root + '/consensust'
    assert(os.path.isfile(consensustExecutive))
    return consensustExecutive



assert(len(sys.argv) == 2)

root = sys.argv[1]

print("Starting tests. Build root:" + sys.argv[1])

consensustExecutive = getConsensustExecutive()

fullConsensusTest("two_out_of_four", consensustExecutive, "[consensus-stuck]")
fullConsensusTest("onenode", consensustExecutive, "[consensus-basic]")
fullConsensusTest("twonodes", consensustExecutive, "[consensus-basic]")
fullConsensusTest("fournodes", consensustExecutive, "[consensus-basic]")
fullConsensusTest("sixteennodes", consensustExecutive, "[consensus-basic]")
fullConsensusTest("fournodes_catchup", consensustExecutive, "[consensus-basic]")
fullConsensusTest("three_out_of_four", consensustExecutive, "[consensus-basic]")





