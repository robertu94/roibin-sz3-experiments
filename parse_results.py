#!/usr/bin/env python

import copy
import re
import csv
import sys
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--input_file", "-i", type=argparse.FileType('r'), default=sys.stdin)
parser.add_argument("--output_file", "-o", type=argparse.FileType('w'), default=sys.stdout)
args = parser.parse_args()

float_pattern=r"(\d+(?:\.\d+)?).*"
NEW_CONFIG = re.compile(r"chunk_size=(\d+) replica=(\d+) config=(\S+) filename=(\S+)")
GLOBAL_CR = re.compile("global_cr=" + float_pattern)
WALLCLOCK_MS = re.compile("wallclock_ms=" + float_pattern)
COMPRESS_MS = re.compile("compress_ms=" + float_pattern)
WALLCLOCK_BW = re.compile("wallclock_bandwidth_GBps=" + float_pattern)
COMPRESS_BW = re.compile("compress_bandwidth_GBps=" + float_pattern)
DECOMPRESS_BW = re.compile("decompress_bandwidth_GBps=" + float_pattern)

result = None
writer = None
for line in args.input_file:
    if m := NEW_CONFIG.match(line):
        if result is not None:
            if writer is None:
                writer = csv.DictWriter(args.output_file, list(result.keys()))
                writer.writeheader()
            writer.writerow(result)
        result = {}
        result["chunk_size"] = int(m.group(1))
        result["replicat"] = m.group(2)
        result["config"] = m.group(3)
        result["filename"] = m.group(4)
        result["decompress_bandwidth_GBps"] = None
        continue
    if m := GLOBAL_CR.match(line):
        result["global_cr"] = float(m.group(1))
        continue
    if m := WALLCLOCK_MS.match(line):
        result["wallclock_ms"] = float(m.group(1))
        continue
    if m := COMPRESS_MS.match(line):
        result["compress_ms"] = float(m.group(1))
        continue
    if m := WALLCLOCK_BW.match(line):
        result["wallclock_bandwidth_GBps"] = float(m.group(1))
        continue
    if m := COMPRESS_BW.match(line):
        result["compress_bandwidth_GBps"] = float(m.group(1))
        continue
    if m := DECOMPRESS_BW.match(line):
        result["decompress_bandwidth_GBps"] = float(m.group(1))
        continue
    if line.startswith("fullscale==="):
        break
    if line.startswith("smallscale==="):
        break
if result is not None:
    writer.writerow(result)
