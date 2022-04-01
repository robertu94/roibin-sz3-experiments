#!/usr/bin/env python

import re
import pandas as pd
from pathlib import Path
CONFIG_LINE = re.compile(r"config=(\S+)")
CHUNK_LINE = re.compile(r"chunk_size=(\d+)")
COMP_BW_LINE = re.compile(r"^compress_bandwidth_GBps=(\d+(?:.\d+)?)")

results = []
with open("roibin_sz.o4751026") as f:
    entry = {}
    for line in f:
        if m := CONFIG_LINE.search(line):
            entry["config_name"] = Path(m.group(1)).stem
            entry["untune"] = "untune-" in entry["config_name"]
            entry["utune_config_name"] = entry["config_name"].removeprefix("untune-")
        if m := CHUNK_LINE.search(line):
            entry["chunk_size"] = int(m.group(1))
        if m := COMP_BW_LINE.search(line):
            entry["compress_bandwidth_GBps"] = float(m.group(1))
            results.append(entry)
            entry = {}
df = pd.DataFrame(results)

tuned = df[df["untune"] == False]
untuned = df[df["untune"] == True]
m = tuned.merge(untuned, on=["utune_config_name","chunk_size"], suffixes=("_tuned", "_untuned"))
m["speedup"] = m["compress_bandwidth_GBps_tuned"]/m["compress_bandwidth_GBps_untuned"]
m["speedup_%"] = (m["speedup"] - 1) * 100


f = m[(m["speedup"] > 1) &( m["chunk_size"] == 16)]
print(f[["utune_config_name", "speedup_%", "compress_bandwidth_GBps_untuned", "compress_bandwidth_GBps_tuned"]].to_latex(index=False))
