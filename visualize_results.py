#!/usr/bin/env python


from pathlib import Path
import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt
pd.set_option('display.max_rows', 500)
sns.set_style("whitegrid")
sns.set(rc = {'figure.figsize':(7,3)})
sns.set_context("paper", font_scale=1.3)

data = pd.read_csv("roibin_sz.o4692185.csv")
data['config'] = data['config'].str[6:-5]
data['filename'] = data['filename'].map(lambda x: str(Path(x).name))

data.describe()

data.groupby(["filename", "config", "chunk_size"]).median("global_cr").sort_values("global_cr")

data = data.sort_values("global_cr")
g = sns.FacetGrid(data=data, row="filename")
g.map_dataframe(sns.barplot, hue="config", x="chunk_size", y="global_cr", palette=sns.color_palette("husl", 19))
g.set(ylim=(0,500))
g.add_legend()
g.tight_layout()
plt.savefig("../figures/global_cr.eps")


data = data.sort_values("compress_bandwidth_GBps")
g = sns.FacetGrid(data=data, row="filename")
g.map_dataframe(sns.barplot, hue="config", x="chunk_size", y="compress_bandwidth_GBps", palette=sns.color_palette("husl", 19))
g.set(ylim=(0,100))
g.add_legend()
g.tight_layout()
plt.savefig("../figures/compress_bw.eps")
