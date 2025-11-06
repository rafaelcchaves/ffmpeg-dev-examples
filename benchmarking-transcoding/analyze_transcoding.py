import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys
import os

def plot_avg_total_time(df):
    """
    Creates and displays a line plot of average total transcoding time vs. total threads.
    """
    df_total = df[df['type'] == 'total'].copy()
    df_total['total_threads'] = df_total['threads_in'] + df_total['threads_out']
    df_total['time_sec'] = df_total['time'] / 1_000_000.0
    
    avg_total_time = df_total.groupby(['total_threads', 'file_name'])['time_sec'].mean().reset_index()
    avg_total_time = avg_total_time.sort_values('total_threads')

    plt.figure(figsize=(12, 8))
    ax = sns.barplot(data=df_total, x='total_threads', y='time_sec', hue='file_name', errorbar=None)
    
    ax.set_title('Transcoding Throughput: Total Time vs. Thread Count')
    ax.set_xlabel('Total Threads (Decoding + Encoding)')
    ax.set_ylabel('Average Total Time (s)')
    ax.grid(True)
    
    print("Displaying average total time line plot...")
    plt.show()

def plot_avg_mean_frame_time(df):
    """
    Creates and displays a line chart of the average mean frame transcoding time vs. total threads.
    """
    df_frames = df[df['type'] == 'transcoding'].copy()
    df_mean_agg = df_frames.groupby(['threads_in', 'threads_out', 'file_name'])['time'].mean().reset_index()
    df_mean_agg['total_threads'] = df_mean_agg['threads_in'] + df_mean_agg['threads_out']
    df_mean_agg['time_ms'] = df_mean_agg['time'] / 1000.0

    avg_mean_frame_time = df_mean_agg.groupby(['total_threads', 'file_name'])['time_ms'].mean().reset_index()
    avg_mean_frame_time = avg_mean_frame_time.sort_values('total_threads')

    plt.figure(figsize=(12, 8))
    ax = sns.barplot(data=df_mean_agg, x='total_threads', y='time_ms', hue='file_name', errorbar=None)
    
    ax.set_title('Transcoding Latency: Mean Frame Time vs. Thread Count')
    ax.set_xlabel('Total Threads (Decoding + Encoding)')
    ax.set_ylabel('Average Mean Frame Time (ms)')
    ax.grid(True)
    
    print("Displaying average mean frame time line chart...")
    plt.show()

def plot_frame_time_boxplot(df):
    """
    Creates and displays a box plot of transcoding frame times.
    """
    df_frames = df[df['type'] == 'transcoding'].copy()
    df_frames['total_threads'] = df_frames['threads_in'] + df_frames['threads_out']
    df_frames['time_ms'] = df_frames['time'] / 1000.0

    plt.figure(figsize=(12, 8))
    ax = sns.boxplot(data=df_frames, x='total_threads', y='time_ms', hue='file_name')
    
    ax.set_title('Transcoding Frame Time Distribution')
    ax.set_xlabel('Total Threads (Decoding + Encoding)')
    ax.set_ylabel('Frame Time (ms)')
    ax.grid(True)
    
    print("Displaying frame time box plot...")
    plt.show()

def analyze_transcoding_files(file_paths):
    """
    Analyzes transcoding data from multiple files, creating line plots for total time
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
    # The 'type' column can have extra single quotes (e.g., "'total'").
    combined_df['type'] = combined_df['type'].str.strip().str.strip("'")
    
    # --- Plotting ---
    plot_frame_time_boxplot(combined_df)
    plot_avg_mean_frame_time(combined_df)
    plot_avg_total_time(combined_df)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        transcoding_files = sys.argv[1:]
    else:
        transcoding_files = ['h264-mjpeg.csv']
        print(f"No input file provided. Defaulting to '{transcoding_files[0]}'.")

    print(f"Starting analysis for {transcoding_files}...")
    analyze_transcoding_files(transcoding_files)
    print("Analysis complete.")