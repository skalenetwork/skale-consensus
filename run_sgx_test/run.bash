#!/bin/bash

PWD=$(pwd)

# Name of the container
CONTAINER_NAME="run_sgx_test-sgxwallet-1"

# Create data directory if it doesn't exist
docker compose pull
docker compose up & 
sleep 1
docker cp ${CONTAINER_NAME}:/usr/src/sdk/sgx_data $PWD
