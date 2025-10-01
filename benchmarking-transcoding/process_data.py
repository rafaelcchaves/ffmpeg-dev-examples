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


sns.set_theme(style="whitegrid")
g = sns.FacetGrid(df, col="type", col_wrap=3, height=5)
def heatmap_facet(*args, **kwargs):
    data = kwargs.pop('data')
    pivot_data = data.pivot_table(index='threads_in', columns='threads_out', values='time')
    sns.heatmap(pivot_data, annot=True, fmt=".1f", cmap="viridis_r", **kwargs)

g.map_dataframe(heatmap_facet)
g.fig.suptitle('Operation Execution Time (us) by Thread Configuration', fontsize=20, weight='bold', y=1.03)
g.set_axis_labels("Encoding Threads", "Decoding Threads", "Time (us)",  fontsize=15)
g.set_titles(col_template="{col_name} operations", size=14)
plt.show()
