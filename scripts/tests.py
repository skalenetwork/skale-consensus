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


def run_without_check(_command):
    print(">" + _command)
    subprocess.call(_command, shell=True)


def run(_command):
    print(">" + _command)
    subprocess.check_call(_command, shell=True)


def unitTest(_consensustExecutive, _testType):
    run("rm -f ./core")
    run(_consensustExecutive + " " + _testType)


def fullConsensusTest(_test, _consensustExecutive, _testType):
    testDir = "test/" + _test
    os.chdir(testDir)
    run("pwd")
    run("rm -rf " + testDir + "/core")
    run("rm -rf /tmp/*.db*")
    run(_consensustExecutive + " " + _testType)
    os.chdir("../..")


def getConsensustExecutive():
    run_without_check("cp -f cmake-build-debug/consensust /tmp/consensust")
    run_without_check("cp -f build/consensust /tmp/consensust")
    consensustExecutive = "/tmp/consensust"
    return consensustExecutive


print("Starting tests.")

os.chdir("..")

run("ccache -M 20G")

consensustExecutive = getConsensustExecutive()

unitTest(consensustExecutive, "[tx-serialize]")
unitTest(consensustExecutive, "[tx-list-serialize]")   


fullConsensusTest("sixteennodes", consensustExecutive, "[consensus-finalization-download]")

# try:
#    fullConsensusTest("two_out_of_four", consensustExecutive, "[consensus-stuck]")
# except:
#    print("Success")


fullConsensusTest("onenode", consensustExecutive, "[consensus-basic]")
fullConsensusTest("twonodes", consensustExecutive, "[consensus-basic]")
fullConsensusTest("fournodes", consensustExecutive, "[consensus-basic]")
fullConsensusTest("sixteennodes", consensustExecutive, "[consensus-basic]")
fullConsensusTest("fournodes_catchup", consensustExecutive, "[consensus-basic]")
fullConsensusTest("three_out_of_four", consensustExecutive, "[consensus-basic]")

unitTest(consensustExecutive, "[tx-serialize]")
unitTest(consensustExecutive, "[tx-list-serialize]")
