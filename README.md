# ROIBIN-SZ Experiments

## Getting started

To build the code, you will need a copy of LibPressio with appropriate dependencies.

### Container Install (for ease of setup)

You can run an example code on a small dataset by running with the following container

```bash
docker pull ghcr.io/robertu94/roibin:latest
docker run -it --rm ghcr.io/robertu94/roibin:latest
```

### Building the container

You can build the container yourself as follows:

```bash
git clone --recursive https://github.com/robertu94/roibin-sz3-experiments
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
