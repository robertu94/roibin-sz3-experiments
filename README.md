# ROIBIN-SZ Experiments

## Getting started

To build the code, you will need a copy of LibPressio with appropriate dependencies.

### Obtaining Data

CXI files can be requested from LCLS from [Chunhong "Chuck" Yoon](https://profiles.stanford.edu/chun-hong-yoon).  We are working on making a subset publicly available.

To run in the container, you may need to set the files to world readable `chmod a+r` to be read inside the container.

### Quality Assessment

The quality analysis results were produced using [PSOCAKE](https://confluence.slac.stanford.edu/display/PSDM/Psocake+SFX+tutorial).
Correct use of this tool requires experience and expertise in serial
crystallography and is outside the scope of this document.


### Container Install (for ease of setup)

We provide a container for `x86_64` image for ease of installation.

This container differs from our experimental setup in 2 ways:

1. The production build used `-march=native -mtune=native` for architecture optimized builds where as the container does not use these flags to maximize compatablity across `x86_64` hardware.
2. We use MPICH in the container rather than the OpenMPI because we found MPICH more reliably ran in the container during testing

NOTE this file is >= 6 GB (without datasets; see above), download with caution.

#### Singularity

You can install and start the container on many super computers using singularity.

```bash
# this first commmand may issue a ton of warnings regarding xattrs depending on your filesystem on your container host; these were benign in our testing.
singularity pull roibin.sif docker://ghcr.io/robertu94/roibin:latest

# -c enables additional confinement than singularity uses by default to prevent polution from /home
# -B bind mounts in the data directory containing your CXI files.
singularity run -c -B path/to/datadir:/data:ro roibin.sif bash
```

#### Docker

You can run an example code on a small dataset by running with the following container and requesting a dataset.

```bash
docker pull ghcr.io/robertu94/roibin:latest
#most systems
docker run -it --rm -v path/to/datadir:/data:ro ghcr.io/robertu94/roibin:latest

# if running on a SeLinux enforcing system
docker run -it --rm --security-opt label=disable -v path/to/datadir:/data:ro roibin
```

### Building the container

You can build the container yourself as follows:
NOTE this process takes 3+ hours on a modern laptop, and most clusters do not
provide sufficient permissions to run container builds on the cluster.

```bash
git clone --recursive https://github.com/robertu94/roibin-sz3-experiments
cd roibin-sz3-experiments
docker build . -t roibin
```

### Manual Install (for scale)

The easiest way to install this manually is with `spack`

```bash
git clone --recursive https://github.com/robertu94/roibin-sz3-experiments
git clone https://github.com/spack/spack
source ./spack/share/spack/setup-env.sh
spack compiler find

spack env activate .
#see note about MPI below
spack install

mkdir build
cd build
cmake ..
```

This software is not compatible with Windows, and hasn't been tested on MacOS.

Please note all functionality will not work on Debian/Ubuntu (due to known bug in LibPressio we hope to resolve soon).
Please use on a RedHat based distribution for testing (i.e. Fedora, CentOS, RHEL, ...).
Additionally some of this code requires a newer compiler and may not compile on older versions of CentOS.

You may wish to configure the build to use your local version of MPI.
Please see [the spack guide](https://spack.readthedocs.io/en/latest/build_settings.html#external-packages) for how to do this.

### Running the Experiments

Once the container is installed, you can run our testing commmands.

```bash
mpiexec -np $procs /app/build/roibin_test -c 1 -f /data/roibin.cxi -p /app/share/roibin_sz.json
```

where `-f` is the input data file, and `-p` is the configuration to use `-c` is the chunk size.

Please see `run_all.sh` for our production configurations.

### Example Output

```console
[demo@620bb069495a app]$ cd /app
[demo@620bb069495a app]$ mpiexec -np 8 ./build/roibin_test -f /data/roibin.cxi -p share/roibin_sz.json -c 32
/pressio/composite/time:time:metric <char*> = "noop"
/pressio/composite:composite:names <char*[]> = {}
/pressio/composite:composite:plugins <char*[]> = {size, time, }
/pressio/composite:composite:scripts <char*[]> = {}
/pressio/roibin/background/composite/time:time:metric <char*> = "noop"
/pressio/roibin/background/composite:composite:names <char*[]> = {}
/pressio/roibin/background/composite:composite:plugins <char*[]> = {size, time, }
/pressio/roibin/background/composite:composite:scripts <char*[]> = {}
/pressio/roibin/background/pressio/composite/time:time:metric <char*> = "noop"
/pressio/roibin/background/pressio/composite:composite:names <char*[]> = {}
/pressio/roibin/background/pressio/composite:composite:plugins <char*[]> = {size, time, }
/pressio/roibin/background/pressio/composite:composite:scripts <char*[]> = {}
/pressio/roibin/background/pressio/sz/composite/time:time:metric <char*> = "noop"
/pressio/roibin/background/pressio/sz/composite:composite:names <char*[]> = {}
/pressio/roibin/background/pressio/sz/composite:composite:plugins <char*[]> = {size, time, }
/pressio/roibin/background/pressio/sz/composite:composite:scripts <char*[]> = {}
/pressio/roibin/background/pressio/sz:metrics:copy_compressor_results <int32> = 1
/pressio/roibin/background/pressio/sz:metrics:errors_fatal <int32> = 1
/pressio/roibin/background/pressio/sz:pressio:abs <double> = 90
/pressio/roibin/background/pressio/sz:pressio:lossless <int32> = <empty>
/pressio/roibin/background/pressio/sz:pressio:metric <char*> = "composite"
/pressio/roibin/background/pressio/sz:pressio:pw_rel <double> = <empty>
/pressio/roibin/background/pressio/sz:pressio:rel <double> = <empty>
/pressio/roibin/background/pressio/sz:sz:abs_err_bound <double> = 90
/pressio/roibin/background/pressio/sz:sz:accelerate_pw_rel_compression <int32> = 1
/pressio/roibin/background/pressio/sz:sz:app <char*> = "SZ"
/pressio/roibin/background/pressio/sz:sz:config_file <char*> = <empty>
/pressio/roibin/background/pressio/sz:sz:config_struct <void*> = <empty>
/pressio/roibin/background/pressio/sz:sz:data_type <double> = <empty>
/pressio/roibin/background/pressio/sz:sz:error_bound_mode <int32> = 0
/pressio/roibin/background/pressio/sz:sz:error_bound_mode_str <char*> = <empty>
/pressio/roibin/background/pressio/sz:sz:exafel:bin_size <uint32> = 0
/pressio/roibin/background/pressio/sz:sz:exafel:calib_panel <data> = data{ type=byte dims={} has_data=false}
/pressio/roibin/background/pressio/sz:sz:exafel:num_peaks <uint32> = 0
/pressio/roibin/background/pressio/sz:sz:exafel:peak_size <uint32> = 0
/pressio/roibin/background/pressio/sz:sz:exafel:peaks_cols <data> = data{ type=byte dims={} has_data=false}
/pressio/roibin/background/pressio/sz:sz:exafel:peaks_rows <data> = data{ type=byte dims={} has_data=false}
/pressio/roibin/background/pressio/sz:sz:exafel:peaks_segs <data> = data{ type=byte dims={} has_data=false}
/pressio/roibin/background/pressio/sz:sz:exafel:sz_dim <uint32> = 0
/pressio/roibin/background/pressio/sz:sz:exafel:tolerance <double> = 0
/pressio/roibin/background/pressio/sz:sz:gzip_mode <int32> = 3
/pressio/roibin/background/pressio/sz:sz:lossless_compressor <int32> = 1
/pressio/roibin/background/pressio/sz:sz:max_quant_intervals <uint32> = 65536
/pressio/roibin/background/pressio/sz:sz:metric <char*> = "composite"
/pressio/roibin/background/pressio/sz:sz:pred_threshold <float> = 0.99
/pressio/roibin/background/pressio/sz:sz:prediction_mode <int32> = <empty>
/pressio/roibin/background/pressio/sz:sz:protect_value_range <int32> = 0
/pressio/roibin/background/pressio/sz:sz:psnr_err_bound <double> = 90
/pressio/roibin/background/pressio/sz:sz:pw_rel_err_bound <double> = 0.001
/pressio/roibin/background/pressio/sz:sz:quantization_intervals <uint32> = 0
/pressio/roibin/background/pressio/sz:sz:rel_err_bound <double> = 0.0001
/pressio/roibin/background/pressio/sz:sz:sample_distance <int32> = 100
/pressio/roibin/background/pressio/sz:sz:segment_size <int32> = 36
/pressio/roibin/background/pressio/sz:sz:snapshot_cmpr_step <int32> = 5
/pressio/roibin/background/pressio/sz:sz:sol_id <int32> = 101
/pressio/roibin/background/pressio/sz:sz:sz_mode <int32> = 1
/pressio/roibin/background/pressio/sz:sz:user_params <void*> = 0
/pressio/roibin/background/pressio:metrics:copy_compressor_results <int32> = 1
/pressio/roibin/background/pressio:metrics:errors_fatal <int32> = 1
/pressio/roibin/background/pressio:pressio:abs <double> = 90
/pressio/roibin/background/pressio:pressio:compressor <char*> = "sz"
/pressio/roibin/background/pressio:pressio:metric <char*> = "composite"
/pressio/roibin/background/pressio:pressio:rel <double> = <empty>
/pressio/roibin/background/pressio:pressio:reset_mode <bool> = <empty>
/pressio/roibin/background:binning:compressor <char*> = "pressio"
/pressio/roibin/background:binning:metric <char*> = "composite"
/pressio/roibin/background:binning:nthreads <uint32> = 4
/pressio/roibin/background:binning:shape <data> = data{ type=double dims={3, } has_data=[2, 2, 1, ]}
/pressio/roibin/background:metrics:copy_compressor_results <int32> = 1
/pressio/roibin/background:metrics:errors_fatal <int32> = 1
/pressio/roibin/background:pressio:metric <char*> = "composite"
/pressio/roibin/composite/time:time:metric <char*> = "noop"
/pressio/roibin/composite:composite:names <char*[]> = {}
/pressio/roibin/composite:composite:plugins <char*[]> = {size, time, }
/pressio/roibin/composite:composite:scripts <char*[]> = {}
/pressio/roibin/roi/composite/time:time:metric <char*> = "noop"
/pressio/roibin/roi/composite:composite:names <char*[]> = {}
/pressio/roibin/roi/composite:composite:plugins <char*[]> = {size, time, }
/pressio/roibin/roi/composite:composite:scripts <char*[]> = {}
/pressio/roibin/roi:fpzip:has_header <int32> = 0
/pressio/roibin/roi:fpzip:metric <char*> = "composite"
/pressio/roibin/roi:fpzip:prec <int32> = 0
/pressio/roibin/roi:metrics:copy_compressor_results <int32> = 1
/pressio/roibin/roi:metrics:errors_fatal <int32> = 1
/pressio/roibin/roi:pressio:metric <char*> = "composite"
/pressio/roibin:metrics:copy_compressor_results <int32> = 1
/pressio/roibin:metrics:errors_fatal <int32> = 1
/pressio/roibin:pressio:metric <char*> = "composite"
/pressio/roibin:roibin:background <char*> = "binning"
/pressio/roibin:roibin:centers <data> = data{ type=byte dims={} has_data=false}
/pressio/roibin:roibin:metric <char*> = "composite"
/pressio/roibin:roibin:nthreads <uint32> = 1
/pressio/roibin:roibin:roi <char*> = "fpzip"
/pressio/roibin:roibin:roi_size <data> = data{ type=double dims={3, } has_data=[8, 8, 0, ]}
/pressio:metrics:copy_compressor_results <int32> = 1
/pressio:metrics:errors_fatal <int32> = 1
/pressio:pressio:compressor <char*> = "roibin"
/pressio:pressio:metric <char*> = "composite"
/pressio:pressio:reset_mode <bool> = <empty>

processing 0 256
processing 256 512
global_cr=79.6186
wallclock_ms=4329
compress_ms=1213
compress_bandwidth_GBps=3.87813
wallclock_bandwidth_GBps=1.08667
```

In this output, the lines beginning with `/pressio` are the represent the configuration used for the experiment.
All of the configurations we used can be found in the `/app/share` directory.
More details on the meanings of these options by calling `pressio -a help <compressor_id>` where the compressor id is one of `binning`, `roi`, `opt`, `fpzip`, `sz`, `sz3`, `zfp`, `mgard`, `blosc`, etc...

the lines `processing <start> <end>` show the progress of each stage of the compression.
For example `processing 0 256` means that the first 256 events are being processed.

`global_cr` is the compression ratio across all events.
`wallclock_ms` is the wall clock time including IO from the CXI file.  In the real system, there would not be the IO from the CXI files.
`compress_ms` is the compression clock time.
`compress_bandwidth_GBps` is the compression bandwidth in GB/s.
`wallclock_bandwidth_GBps` is the wallclock bandwidth in GB/s
