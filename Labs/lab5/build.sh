#!/bin/sh

python3 -m grpc_tools.protoc -I../grpc/protos --python_out=. --grpc_python_out=. ./route_guide.proto
