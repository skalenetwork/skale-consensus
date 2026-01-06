#!/bin/bash

# Name of the container
CONTAINER_NAME="run_sgx_test-sgxwallet-1"

# Stop and remove existing container if running
if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
  echo "Stopping and removing existing container: $CONTAINER_NAME"
  PID=$(docker inspect --format '{{.State.Pid}}' "$CONTAINER_NAME" 2>/dev/null)
  sudo kill -9 "$PID"
  docker rm -f "$CONTAINER_NAME"
fi


