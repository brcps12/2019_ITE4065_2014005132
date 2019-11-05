#!/bin/bash
NUM_THREADS=(1 2 4 8 16 32)

for N in ${NUM_THREADS[@]};
do
    echo "=== Begin with N=${N} ==="
    ./run ${N}
done
