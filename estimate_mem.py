#!/usr/bin/env python

z1=3 #sz mem multiple estimate
z2=3 #fpzip mem multiple estimate
bx=2   # bin size
by=2   # bin size
rx=17  # roi size x (note 2x+1 since this is what the code does)
ry=17  # roi size y
x=1480 # x dim
y=1552 # y dim
s=4    # float32 are 4 bytes
m=2048 # max peaks in a single chunk

def to_human_size(byte):
    if byte < 1024:
        return f"{byte}b"
    elif byte < 1024**2:
        return f"{byte/1024}kb"
    elif byte < 1024**3:
        return f"{byte/1024**2}mb"
    else:
        return f"{byte/1024**3}gb"

def roibin_mem_estimate(c, cores=1):
    return cores * c * s * ((1+(2*z1)/(bx*by))*x*y + (1+z2)*rx*ry*m + 7*m + 1)

#validation, my laptop
to_human_size(roibin_mem_estimate(32, cores=1)) #should be about 1gb
to_human_size(roibin_mem_estimate(8, cores=8)) #should be about 1gb
to_human_size(roibin_mem_estimate(512, cores=1)) #should be about 15gb

#lcrc machines
to_human_size(roibin_mem_estimate(64+32, cores=36)) # for broadwell nodes, use 128 sized chunks -- 128GB ram avail
to_human_size(roibin_mem_estimate(32+8, cores=64)) # for knl nodes, use 64 sized chunks -- 96GB ram avail

#alcf machines
to_human_size(roibin_mem_estimate(64+16, cores=64)) # for theta nodes, 192GB ram avail

# slac machines
to_human_size(roibin_mem_estimate(128+64, cores=16)) # for batch queue 16 core nodes with 128GB ram
to_human_size(roibin_mem_estimate(32+16, cores=12)) # for batch queue 12 core nodes -- 24 GB ram


