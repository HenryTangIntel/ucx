#!/bin/bash
# Script to build and run the Gaudi memory domain test

set -e

echo "Building test_gaudi_md..."
make -f test_gaudi_md.Makefile

echo "Running Gaudi MD test..."
./test_gaudi_md
