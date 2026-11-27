#!/usr/bin/env python3
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


import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from urllib.error import URLError
from urllib.request import Request, urlopen


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
SGX_DIR = Path(REPO_ROOT) / "run_sgx_test"


def print_separator(_label, _char="="):
    line = _char * 80
    print("\n" + line)
    print(_label)
    print(line)


def normalize_catch_expression(_testType):
    if len(_testType) >= 2 and _testType[0] == _testType[-1] and _testType[0] in ("'", '"'):
        return _testType[1:-1]
    return _testType


def list_catch_test_names(_consensustExecutive, _testType, cwd):
    result = subprocess.run(
        [_consensustExecutive, normalize_catch_expression(_testType), "--list-test-names-only"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        cwd=cwd,
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


def run_args(_command, _label=None, cwd=REPO_ROOT):
    if _label:
        print_separator(_label)
    print(">" + " ".join(_command))
    subprocess.check_call(_command, cwd=cwd)


def run_catch_tests(_consensustExecutive, _testType, _label, cwd=REPO_ROOT):
    # By default, split catch tests into separate runs
    # There are currently static variables that bleed state between tests
    # if we try running all under the same catch session, causing tests to fail

    test_names = list_catch_test_names(_consensustExecutive, _testType, cwd)
    if len(test_names) == 0:
        raise RuntimeError("No Catch tests matched: " + _testType)
    print_separator(_label + " (" + str(len(test_names)) + " cases)")

    for index, test_name in enumerate(test_names, 1):
        run_args(
            # use '"' delimiters to ensure test names with commas are treated as a single argument
            [_consensustExecutive, '"' + test_name + '"'],
            "[{}/{}] {}".format(index, len(test_names), test_name),
            cwd=cwd,
        )


def fullConsensusTest(_test, _consensustExecutive, _testType):
    testDir = os.path.join(REPO_ROOT, "test", _test)
    run_catch_tests(
        _consensustExecutive,
        _testType,
        "Consensus test: " + _test + " " + _testType,
        cwd=testDir,
    )


def getConsensustExecutive():
    return os.path.join(REPO_ROOT, "build", "consensust")


def compose(*args, check=True, capture_output=False):
    return subprocess.run(
        ["docker", "compose", *args],
        cwd=SGX_DIR,
        check=check,
        capture_output=capture_output,
        text=True,
        timeout=120,
    )


def wait_for_sgx(timeout):
    deadline = time.monotonic() + timeout
    last_error = "test keys have not been generated"
    while time.monotonic() < deadline:
        running = compose("ps", "--status", "running", "-q", "sgxwallet", capture_output=True)
        if not running.stdout.strip():
            raise RuntimeError("SGX simulator is not running")
        try:
            for name in ("4node.json", "16node.json"):
                with (SGX_DIR / "sgx_data" / name).open() as key_file:
                    keys = json.load(key_file)
                if not isinstance(keys, dict) or not keys.get("ecdsaKeyNames") or not keys.get("blsKeyNames"):
                    raise ValueError("Incomplete SGX test keys: " + name)
            logs = compose("logs", "--no-color", "sgxwallet", capture_output=True).stdout
            if "Started zmq read loop" not in logs or not any(
                marker in logs for marker in (
                    "Successfully completed generating test keys", "Found test keys"
                )
            ):
                raise ValueError("Waiting for ZMQ startup and completed test-key generation")
            request = Request(
                "http://localhost:1029",
                data=json.dumps({
                    "jsonrpc": "2.0", "id": 1, "method": "getServerStatus", "params": None
                }).encode("utf-8"),
                headers={"Content-Type": "application/json"},
            )
            with urlopen(request, timeout=5) as response:
                status = json.load(response)
            result = status.get("result") if isinstance(status, dict) else None
            if not isinstance(result, dict) or result.get("status") != 0:
                raise ValueError("SGX health RPC failed: " + json.dumps(status))
            print("SGX simulator and test keys are ready", flush=True)
            return
        except (OSError, URLError, ValueError) as error:
            last_error = str(error)
        time.sleep(min(5, max(0, deadline - time.monotonic())))
    raise RuntimeError("Timed out waiting for SGX simulator: " + last_error)


@contextmanager
def sgx_simulator(timeout, external):
    started_here = False
    try:
        running = compose("ps", "--status", "running", "-q", "sgxwallet", capture_output=True)
        if not running.stdout.strip():
            if external:
                raise RuntimeError("--external-sgx requires a running run_sgx_test Compose service")
            compose("pull", "sgxwallet")
            started_here = True
            compose("up", "-d", "sgxwallet")
        wait_for_sgx(timeout)
        yield
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, KeyboardInterrupt):
        try:
            compose("logs", "--no-color", "--tail=200", "sgxwallet")
        except (OSError, subprocess.SubprocessError) as error:
            print("Could not retrieve SGX logs: " + str(error), file=sys.stderr)
        raise
    finally:
        if started_here:
            compose("stop", "sgxwallet")


def main():
    parser = argparse.ArgumentParser(
        description="Run Catch cases in separate processes; never builds the test binary.",
        epilog="E2E uses run_sgx_test/docker-compose.yml. Existing services are reused "
               "and left running; services started here are stopped without deleting wallet data.",
    )
    parser.add_argument(
        "mode", nargs="?", default="all", choices=("unit", "e2e", "performance", "all"),
        help="'all' runs unit and E2E correctness tests, not performance or legacy tests",
    )
    parser.add_argument("--binary", help="Path to an existing consensust executable")
    parser.add_argument(
        "--external-sgx", action="store_true",
        help="Require an already-running shared Compose simulator; do not start or stop it",
    )
    parser.add_argument(
        "--sgx-timeout", type=int, default=600, metavar="SECONDS",
        help="Simulator readiness timeout (default: 600)",
    )
    args = parser.parse_args()
    if args.sgx_timeout <= 0:
        parser.error("--sgx-timeout must be positive")
    binary = os.path.abspath(args.binary) if args.binary else getConsensustExecutive()
    if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
        raise RuntimeError("Test binary is missing or not executable: " + binary)

    if args.mode in ("unit", "all"):
        run_catch_tests(binary, "~[end-to-end]~[performance]~[legacy]", "Unit/component tests")
    if args.mode in ("e2e", "all"):
        with sgx_simulator(args.sgx_timeout, args.external_sgx):
            run_catch_tests(
                binary, "[end-to-end]~[performance]~[legacy]~[consensus-basic]", "E2E tests"
            )
            for configuration in ("onenode", "twonodes", "fournodes"):
                fullConsensusTest(
                    configuration, binary, "[consensus-basic]~[legacy]~[performance]"
                )
    if args.mode == "performance":
        run_catch_tests(binary, "[performance]~[legacy]", "Performance tests")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print("Test runner failed: " + str(error), file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)
