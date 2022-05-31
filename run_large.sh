#!/usr/bin/env bash
#PBS -N roibin_sz_large
#PBS -l select=10:ncpus=40:mem=370gb:interconnect=hdr:mpiprocs=40,walltime=72:00:00
#PBS -m abe
#PBS -M robertu@clemson.edu
#PBS -j oe

workdir=/home/robertu/scratch/roibin-sz3-experiments
cd $workdir

# performance assessment
source /home/robertu/git/spack/share/spack/setup-env.sh
spack env activate .
module load openmpi/4.0.5-gcc/8.4.1-ucx

chunk_size=1
replica=1
for cxi_file in /scratch1/robertu/roibin_full/*/*.cxi
do
	for config in share/table2/b3_abs90_dim2.json share/table2/b2_abs90_dim3.json
	do
		if [ ! -f "$cxi_file.$(basename $config)" ]; then
			echo "chunk_size=$chunk_size replica=$replica config=$config filename=$cxi_file"
			./build/timeout 1800 mpiexec ./build/roibin_test -c $chunk_size -f $cxi_file -o $cxi_file.$(basename $config) -p $config
		else
			echo "SKIPPED chunk_size=$chunk_size replica=$replica config=$config filename=$cxi_file"
		fi
	done
done
