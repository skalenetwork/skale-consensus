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


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))


def print_separator(_label, _char="="):
    line = _char * 80
    print("\n" + line)
    print(_label)
    print(line)


def normalize_catch_expression(_testType):
    if len(_testType) >= 2 and _testType[0] == _testType[-1] and _testType[0] in ("'", '"'):
        return _testType[1:-1]
    return _testType


def list_catch_test_names(_consensustExecutive, _testType):
    result = subprocess.run(
        [_consensustExecutive, normalize_catch_expression(_testType), "--list-test-names-only"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
    )
    test_names = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if len(test_names) == 0:
        error_output = result.stderr.strip()
        raise RuntimeError(
            "Could not list Catch tests for {} (exit code {}): {}".format(
                _testType,
                result.returncode,
                error_output or "no tests matched",
            )
        )
    return test_names


def run(_command, _label=None):
    if _label:
        print_separator(_label)
    print(">" + _command)
    subprocess.check_call(_command, shell=True)


def run_args(_command, _label=None):
    if _label:
        print_separator(_label)
    print(">" + " ".join(_command))
    subprocess.check_call(_command)


def run_catch_tests(_consensustExecutive, _testType, _label):
    # By default, split catch tests into separate runs
    # There are currently static variables that bleed state between tests
    # if we try running all under the same catch session, causing tests to fail

    test_names = list_catch_test_names(_consensustExecutive, _testType)
    if len(test_names) == 0:
        raise RuntimeError("No Catch tests matched: " + _testType)
    print_separator(_label + " (" + str(len(test_names)) + " cases)")

    for index, test_name in enumerate(test_names, 1):
        run_args(
            # use '"' delimiters to ensure test names with commas are treated as a single argument
            [_consensustExecutive, '"' + test_name + '"'],
            "[{}/{}] {}".format(index, len(test_names), test_name),
        )


def unitTest(_consensustExecutive, _testType, _label=None):
    label = _label or _testType
    run("rm -f ./core", "Cleanup for " + label)
    run_catch_tests(_consensustExecutive, _testType, "Catch tests: " + label)


def fullConsensusTest(_test, _consensustExecutive, _testType):
    testDir = os.path.join(REPO_ROOT, "test", _test)
    os.chdir(testDir)
    run("pwd", "Entering " + testDir)
    run("rm -rf ./core", "Cleanup core files for " + _test)
    run("rm -rf /tmp/*.db*", "Cleanup db files for " + _test)
    run_catch_tests(
        _consensustExecutive,
        _testType,
        "Consensus test: " + _test + " " + _testType,
    )
    os.chdir(REPO_ROOT)


def getConsensustExecutive():
    candidates = [
        os.path.join(REPO_ROOT, "cmake-build-debug", "consensust"),
        os.path.join(REPO_ROOT, "build", "consensust"),
    ]

    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate

    raise RuntimeError(
        "Could not find consensust binary. Looked in: {}".format(", ".join(candidates))
    )


print("Starting tests.")

os.chdir(REPO_ROOT)

run("ccache -M 20G")

consensustExecutive = getConsensustExecutive()

# Run all non-end-to-end and non-performance unit tests
unitTest(
    consensustExecutive,
    "~[end-to-end]~[performance]",
    "Non-end-to-end non-performance",
)

unitTest(
    consensustExecutive,
    "'[end-to-end][db]'",
    "End-to-end db tests",
)

unitTest(
    consensustExecutive,
    "[blockfinalize-transport]",
    "Blockfinalize backward compatibility tests",
)


fullConsensusTest("onenode", consensustExecutive, "[consensus-basic]")
fullConsensusTest("twonodes", consensustExecutive, "[consensus-basic]")
fullConsensusTest("fournodes", consensustExecutive, "[consensus-basic]")
