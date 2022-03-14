#!/usr/bin/env bash

workdir=/home/robertu/scratch/roibin-sz3-experiments
cd $workdir

source /home/robertu/git/spack/share/spack/setup-env.sh
spack env activate .

for cxi_file in "/scratch1/robertu/chuck/cxic00318_0123_0.cxi" "/scratch1/robertu/chuck/cxic0415_0101.cxi"
do 
for chunk_size in 1 8
do
for replica in `seq 3`
do
for config in share/*.json
do
  echo "chunk_size=$chunk_size replica=$replica config=$config filename=$cxi_file"
  mpiexec ./build/roibin_test -c $chunk_size -f "$cxi_file" -p "$config"
done
done
done
done
