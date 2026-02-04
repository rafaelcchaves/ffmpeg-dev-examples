import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys
import os

# Define profile order and display names
PROFILE_ORDER = ['low_latency', 'balanced', 'high_throughput']
PROFILE_DISPLAY_NAMES = {
    'low_latency': 'Low Latency',
    'balanced': 'Balanced',
    'high_throughput': 'High Throughput'
}

# Define codec display names for transformation labels
CODEC_NAMES = {
    'avc': 'AVC',
    'hevc': 'HEVC',
    'mjpeg': 'MJPEG',
    'lz4': 'LZ4',
    'libsvtjpegxs': 'JpegXS'
}

def get_transformation_label(file_name: str) -> str:
    """Convert filename to transformation label (e.g., 'hevc_mjpeg' -> 'HEVC -> MJPEG')."""
    parts = file_name.replace('_', '-').split('-')
    if len(parts) >= 2:
        source = CODEC_NAMES.get(parts[0], parts[0].upper())
        dest = CODEC_NAMES.get(parts[1], parts[1].upper())
        return f'{source} -> {dest}'
    return file_name.upper()

def plot_avg_fps_by_profile(df):
    """
    Creates and displays a bar plot of average FPS by profile.
    """
    df_fps = df[df['type'] == 'fps'].copy()
    df_fps['transformation'] = df_fps['file_name'].apply(get_transformation_label)

    plt.figure(figsize=(12, 8))
    ax = sns.barplot(data=df_fps, x='profile', y='time', hue='transformation',
                     order=PROFILE_ORDER, errorbar=None)

    ax.set_title('Transcoding Throughput: FPS by Profile')
    ax.set_xlabel('Profile')
    ax.set_ylabel('Average Frames Per Second (FPS)')
    ax.grid(True, axis='y')

    # Set custom x-axis labels to show only profile names
    ax.set_xticks(range(len(PROFILE_ORDER)))
    ax.set_xticklabels([PROFILE_DISPLAY_NAMES[p] for p in PROFILE_ORDER])

    print("Displaying average FPS by profile bar plot...")
    plt.tight_layout()
    plt.show()

def plot_avg_mean_frame_time_by_profile(df):
    """
    Creates and displays a bar chart of the average mean frame transcoding time by profile.
    """
    df_frames = df[df['type'] == 'transcoding'].copy()
    df_frames['transformation'] = df_frames['file_name'].apply(get_transformation_label)
    df_mean_agg = df_frames.groupby(['profile', 'transformation'])['time'].mean().reset_index()
    df_mean_agg['time_ms'] = df_mean_agg['time'] / 1000.0

    plt.figure(figsize=(12, 8))
    ax = sns.barplot(data=df_mean_agg, x='profile', y='time_ms', hue='transformation',
                     order=PROFILE_ORDER, errorbar=None)

    ax.set_title('Transcoding Latency: Mean Frame Time by Profile')
    ax.set_xlabel('Profile')
    ax.set_ylabel('Average Mean Frame Time (ms)')
    ax.grid(True, axis='y')

    # Set custom x-axis labels to show only profile names
    ax.set_xticks(range(len(PROFILE_ORDER)))
    ax.set_xticklabels([PROFILE_DISPLAY_NAMES[p] for p in PROFILE_ORDER])

    print("Displaying average mean frame time by profile bar chart...")
    plt.tight_layout()
    plt.show()

def plot_frame_time_boxplot_by_profile(df):
    """
    Creates and displays a box plot of transcoding frame times by profile.
    """
    df_frames = df[df['type'] == 'transcoding'].copy()
    df_frames['time_ms'] = df_frames['time'] / 1000.0
    df_frames['transformation'] = df_frames['file_name'].apply(get_transformation_label)

    plt.figure(figsize=(12, 8))
    ax = sns.boxplot(data=df_frames, x='profile', y='time_ms', hue='transformation',
                     order=PROFILE_ORDER)

    ax.set_title('Transcoding Frame Time Distribution by Profile')
    ax.set_xlabel('Profile')
    ax.set_ylabel('Frame Time (ms)')
    ax.grid(True, axis='y')

    # Set custom x-axis labels to show only profile names
    ax.set_xticks(range(len(PROFILE_ORDER)))
    ax.set_xticklabels([PROFILE_DISPLAY_NAMES[p] for p in PROFILE_ORDER])

    print("Displaying frame time box plot by profile...")
    plt.tight_layout()
    plt.show()

def analyze_transcoding_files(file_paths):
    """
    Analyzes transcoding data from multiple files, creating plots by profile.
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

    # Print summary statistics by profile
    print("\n=== Summary Statistics by Profile ===")
    for profile in PROFILE_ORDER:
        profile_df = combined_df[combined_df['profile'] == profile]
        if not profile_df.empty:
            fps_data = profile_df[profile_df['type'] == 'fps']['time']
            if not fps_data.empty:
                print(f"\n{PROFILE_DISPLAY_NAMES[profile]}:")
                print(f"  Average FPS: {fps_data.mean():.2f}")

    # --- Plotting ---
    plot_frame_time_boxplot_by_profile(combined_df)
    plot_avg_mean_frame_time_by_profile(combined_df)
    plot_avg_fps_by_profile(combined_df)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        transcoding_files = sys.argv[1:]
    else:
        transcoding_files = ['h264-mjpeg.csv']
        print(f"No input file provided. Defaulting to '{transcoding_files[0]}'.")

    print(f"Starting analysis for {transcoding_files}...")
    analyze_transcoding_files(transcoding_files)
    print("Analysis complete.")
