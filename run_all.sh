#!/usr/bin/env bash
#PBS -N roibin_sz
#PBS -l select=8:ncpus=40:mem=370gb:interconnect=hdr:mpiprocs=30,walltime=24:00:00
#PBS -m abe
#PBS -M robertu@clemson.edu
#PBS -j oe

workdir=/home/robertu/scratch/roibin-sz3-experiments
cd $workdir

# performance assessment
source /home/robertu/git/spack/share/spack/setup-env.sh
spack env activate .
module load openmpi/4.0.5-gcc/8.4.1-ucx

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

echo smallscale===
# small-scale quality assessment
chunk_size=1
cxi_file=/scratch1/robertu/chuck/cxic00318_0123_0.cxi
for config in share/roibin_zfp.json share/roibin_mgard.json share/sz.json share/roibin_blosc-9.json share/sz.json share/sz3.json share/mgard.json share/zfp.json
do
	mpiexec ./build/roibin_test -c $chunk_size -f "$cxi_file" -o "$cxi_file.$(basename $config)" -p "$config"
done

echo fullscale===
# full-scale quality assessment
config=./share/roibin_sz.json
chunk_size=1
for cxi_file in /scratch1/robertu/chuck/full_eval/cxic00318_0123_*.cxi
do
	mpiexec ./build/roibin_test -c $chunk_size -f "$cxi_file" -o "$cxi_file.$(basename $config)" -p "$config"
done
