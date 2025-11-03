#!/bin/bash

# --- Configuration ---
# Set the name of your raw input file
INPUT_FILE="original.yuv"

# Input parameters
IN_SIZE="3840x2160"
IN_PIX_FMT="yuv420p"
IN_FPS="50"

# Output parameters
OUT_FPS="50"

if [[ -z $1 ]]; then
	OUT_SIZE="3840x2160"
else
	OUT_SIZE=$1
fi

# --- End of Configuration ---

# This combines all input flags into one variable for readability
INPUT_FLAGS="-f rawvideo -r $IN_FPS -pixel_format $IN_PIX_FMT -video_size $IN_SIZE -i $INPUT_FILE"

# --- Start Encoding ---
# FFmpeg will process these jobs one by one.

echo "-----------------------------------"
echo "Starting VVC (vvenc) encode... (Preset medium)"
ffmpeg $INPUT_FLAGS -vf  scale=$OUT_SIZE -c:v libvvenc -preset medium -r $OUT_FPS ${OUT_SIZE}_vvc.mp4

echo "Starting AV1 (libsvtav1) encode... (Preset 6)"
ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libsvtav1 -preset 6 -r $OUT_FPS ${OUT_SIZE}_av1.mp4

echo "-----------------------------------"
echo "Starting VP9 (libvpx-vp9) encode... (CPU-Used 4)"
# -b:v 0 is required for CRF mode in libvpx-vp9
ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libvpx-vp9 -cpu-used 4 -b:v 0 -r $OUT_FPS ${OUT_SIZE}_vp9.mp4

echo "-----------------------------------"
echo "Starting AVC (libx264) encode... (Preset medium)"
ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libx264 -preset medium -r $OUT_FPS ${OUT_SIZE}_avc.mp4

echo "-----------------------------------"
echo "Starting HEVC (libx265) encode... (Preset medium)"
ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libx265 -preset medium -r $OUT_FPS ${OUT_SIZE}_hevc.mp4

echo "-----------------------------------"
echo "All encoding jobs are complete."
