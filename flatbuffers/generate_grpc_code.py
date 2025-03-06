#!/usr/bin/env python3
import subprocess



def run_command():
    command = "../deps/grpc/build/third_party/protobuf/protoc     --grpc_out=.     --cpp_out=.     --plugin=protoc-gen-grpc=../deps/grpc/build/grpc_cpp_plugin     ./block_finalize.proto"

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
