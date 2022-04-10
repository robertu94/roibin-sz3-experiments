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

You can run an example code on a small dataset by running with the following container and requesting a dataset.
NOTE this file is >= 6 GB (without datasets; see above), download with caution.

```bash
docker pull ghcr.io/robertu94/roibin:latest
#most systems
docker run -it --rm -v path/to/data:/data:ro ghcr.io/robertu94/roibin:latest

# if running on a SeLinux enforcing system
docker run -it --rm --security-opt label=disable -v path/to/data:/data:ro roibin
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

```bash
./build/roibin_test -c 1 -f /data/roibin.cxi -p ./share/roibin_sz.json
```

where `-f` is the input data file, and `-p` is the configuration to use.

Please see `run_all.sh` for our production configurations.
