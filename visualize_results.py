#!/usr/bin/env python


import seaborn as sns
import pandas as pd
pd.set_option('display.max_rows', 500)

data = pd.read_csv("roibin_sz.o4692185.csv")

data.describe()

data.groupby(["filename", "config", "chunk_size"]).median("global_cr").sort_values("global_cr")

