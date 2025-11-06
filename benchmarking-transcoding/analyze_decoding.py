try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import seaborn as sns
except ImportError as e:
    print(f"Import error: {e}")
    print("Please install the required libraries: pip install pandas matplotlib seaborn")
    sys.exit(1)

import sys
import os

# Define thread levels for correct sorting on charts
THREAD_LEVELS = [1, 2, 4, 8, 12, 16]

def plot_total_time(df):
    """
    Creates and displays a line plot of total decoding time by input threads.
    """
    df_total = df[df['type'] == 'total'].copy()
    df_total['time_sec'] = df_total['time'] / 1_000_000.0
    
    plt.figure()
    used_threads = sorted(df_total['threads_in'].unique())
    df_total['threads_in'] = pd.Categorical(df_total['threads_in'], categories=used_threads, ordered=True)
    ax = sns.barplot(data=df_total, x='threads_in', y='time_sec', hue='file_name', errorbar=None)
    
    ax.set_title('Decoding Throughput: Total Time vs. Decoding Thread Count')
    ax.set_xlabel('Decoding Threads')
    ax.set_ylabel('Total Time (s)')
    ax.grid(True)
    
    print("Displaying total time line plot...")
    plt.show()

def plot_mean_frame_time(df):
    """
    Creates and displays a line chart of the mean frame decoding time for each thread level.
    """
    df_frames = df[df['type'] == 'decoding'].copy()
    df_frames['time_ms'] = df_frames['time'] / 1000.0
    
    plt.figure()
    used_threads = sorted(df_frames['threads_in'].unique())
    df_frames['threads_in'] = pd.Categorical(df_frames['threads_in'], categories=used_threads, ordered=True)
    ax = sns.barplot(data=df_frames, x='threads_in', y='time_ms', hue='file_name', errorbar=None)
    
    ax.set_title('Decoding Latency: Mean Frame Time vs. Decoding Thread Count')
    ax.set_xlabel('Decoding Threads')
    ax.set_ylabel('Mean Frame Time (ms)')
    ax.grid(True)
    
    print("Displaying mean frame time line chart...")
    plt.show()

def plot_frame_time_boxplot(df):
    """
    Creates and displays a box plot of decoding frame times.
    """
    df_frames = df[df['type'] == 'decoding'].copy()
    df_frames['time_ms'] = df_frames['time'] / 1000.0

    plt.figure(figsize=(12, 8))
    used_threads = sorted(df_frames['threads_in'].unique())
    df_frames['threads_in'] = pd.Categorical(df_frames['threads_in'], categories=used_threads, ordered=True)
    ax = sns.boxplot(data=df_frames, x='threads_in', y='time_ms', hue='file_name')
    
    ax.set_title('Decoding Frame Time Distribution')
    ax.set_xlabel('Decoding Threads')
    ax.set_ylabel('Frame Time (ms)')
    ax.grid(True)
    
    print("Displaying frame time box plot...")
    plt.show()

def analyze_decoding(file_paths):
    """
    Analyzes the decoding data from multiple files, creating line plots for total time
    and mean frame time.
    """
    sns.set_theme(style="whitegrid")
    plt.rcParams.update({'font.size': 12})

    all_dfs = []
    for file_path in file_paths:
        try:
            df = pd.read_csv(file_path)
            df['file_name'] = os.path.splitext(os.path.basename(file_path))[0]
            all_dfs.append(df)
        except FileNotFoundError:
            print(f"Error: Could not find file '{file_path}'")
            continue
        except Exception as e:
            print(f"An error occurred while reading the file '{file_path}': {e}")
            continue
    
    if not all_dfs:
        print("No valid data files found.")
        sys.exit(1)

    combined_df = pd.concat(all_dfs, ignore_index=True)

    # --- Data Cleaning ---
    combined_df['type'] = combined_df['type'].str.strip().str.strip("'")
    
    # --- Plotting ---
    plot_frame_time_boxplot(combined_df)
    plot_mean_frame_time(combined_df)
    plot_total_time(combined_df)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        decoding_files = sys.argv[1:]
    else:
        decoding_files = ['h264.csv']
        print(f"No input file provided. Defaulting to '{decoding_files[0]}'.")

    print(f"Starting analysis for {decoding_files}...")
    analyze_decoding(decoding_files)
    print("Analysis complete.")
