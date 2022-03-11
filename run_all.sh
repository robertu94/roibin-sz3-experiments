#!/usr/bin/env bash

for cxi_file in "$HOME/git/datasets/roibin.cxi"
do
for chunk_size in 1 8
do
for replica in `seq 3`
do
for config in share/*.json
do
  echo "chunk_size=$chunk_size replica=$replica config=$config"
  mpiexec ./build/roibin_test -c $chunk_size -f "$cxi_file" -p "$config"
done
done
done
done
