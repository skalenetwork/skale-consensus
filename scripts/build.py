#!/usr/bin/env python3

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
import argparse



def run(_command):
    print(">" +_command)
    subprocess.check_call(_command, shell = True)


def main():
    parser = argparse.ArgumentParser(description="Example script for argument parsing")
    parser.add_argument("buildType", type=str, help="Build type (release, debug)")
    parser.add_argument("-buildPl", action = "store_true", help="Build PL")

    os.chdir("..")
    print("Starting build")
    buildType = sys.buildType
    print("BUILD_TYPE=" + buildType)

    buildPL = sys.buildPl;

    run("ccache -M 20G")
    run("mkdir -p build")
    os.chdir("build")

    command : str = "cmake .. -DCMAKE_BUILD_TYPE=" + buildType + " -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0"
    if buildPL:
        command += " -DPL=1"

    run("cmake .. -DCMAKE_BUILD_TYPE=" + buildType +
        " -DCOVERAGE=ON -DMICROPROFILE_ENABLED=0")
    run("make -j$(nproc)")
    os.chdir("..")
    assert os.path.isfile("build/consensust")
    assert os.path.isfile("build/consensusd")
    print("Build successfull.")


if __name__ == "__main__":
    main()



