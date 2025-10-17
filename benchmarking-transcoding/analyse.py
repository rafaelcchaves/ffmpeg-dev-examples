import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import sys

csv_path = 'benchmarking.csv' if len(sys.argv) == 1 else sys.argv[1]
try:
    df = pd.read_csv(csv_path)
    print("CSV file loaded successfully. Here's a preview:")
except FileNotFoundError:
    print(f"Error: '{csv_path}' not found. Please make sure the file exists in the same directory.")
    exit(0)

df["time"] = df["time"]/1000



sns.set_theme(style="whitegrid")
g = sns.FacetGrid(df, col="type", col_wrap=2, height=5)
def heatmap_facet(*args, **kwargs):
    data = kwargs.pop('data')
    pivot_data = data.pivot_table(index='threads_in', columns='threads_out', values='time')
    print(pivot_data)
    sns.heatmap(pivot_data, annot=True, fmt=".1f", cmap="viridis_r", **kwargs)

g.map_dataframe(heatmap_facet)
g.fig.suptitle('Execution Time (ms) by Thread Configuration', fontsize=15, weight = "bold")
g.set_axis_labels("Encoding Threads", "Decoding Threads", "Time (ms)",  fontsize=15)
g.set_titles(col_template="{col_name}", size=14)
plt.show()
