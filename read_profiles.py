#!/usr/bin/env python

import argparse
import json
import os
import pandas as pd
from pprint import pprint
from pathlib import Path

pd.set_option('display.max_columns', 500)

parser = argparse.ArgumentParser()
parser.add_argument("--input_directory", type=Path, default=Path(os.environ.get("TMPDIR", "/tmp")))
args = parser.parse_args()

class args:
    input_directory = Path("/tmp")


def search(pattern):
    compression_times = []
    compression_ratios = []
    for dir in args.input_directory.glob(pattern):
        with dir.open() as dir_f:
            profile = json.load(dir_f)
            entries = {}
            for key in profile:
                if not key.endswith("time:compress"):
                    continue
                component = "/".join(i for i in key.split(':')[0].split('/')[:-2])
                if isinstance(profile[key], dict):
                    entries[component] = profile[key]['value']
                else:
                    entries[component] = profile[key]
            compression_times.append(entries)
            entries = {}
            for key in profile:
                if not key.endswith("size:compression_ratio"):
                    continue
                component = "/".join(i for i in key.split(':')[0].split('/')[:-2])
                if isinstance(profile[key], dict):
                    entries[component] = profile[key]['value']
                else:
                    entries[component] = profile[key]
            compression_ratios.append(entries)
    compression_times = pd.DataFrame(compression_times)
    compression_ratios = pd.DataFrame(compression_ratios)
    return compression_times, compression_ratios


untuned_pattern = "roibin.cxi-untune-roibin_sz.json-*.json"
tuned_pattern = "roibin.cxi-roibin_sz.json-*.json"
uct, ucr = search(untuned_pattern)
ct, cr = search(tuned_pattern)
uct.describe() - ct.describe()

compression_times.describe()

compression_ratios.describe()
