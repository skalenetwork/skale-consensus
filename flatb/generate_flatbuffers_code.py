#!/usr/bin/env python3
import subprocess



def run_command():
    command = "../deps/flatbuffers/build2/flatc --cpp  --gen-object-api *.fbs"

    try:
        result = subprocess.run(command, shell=True, capture_output=True, text=True)

        print("Output:")
        print(result.stdout if result.stdout else "No output")

        print("Error:")
        print(result.stderr if result.stderr else "No error")

    except Exception as e:
        print(f"Exception occurred: {e}")


if __name__ == "__main__":
    run_command()
