#!/bin/bash

# --- Usage ---
# Extract SSIM value between two files (raw YUV or compressed)
# Usage: get_ssim.sh <resolution> <reference.yuv> <compressed_file>
# Returns: SSIM value as decimal (e.g., 0.951234)

if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <width>x<height> <reference.yuv> <compressed_file>"
    exit 1
fi

RESOLUTION="$1"
REF_FILE="$2"
COMPRESSED_FILE="$3"
TEMP_RAW="/tmp/get_ssim_$$_decoded.yuv"

# Decode compressed file to raw YUV
ffmpeg -loglevel quiet -i "$COMPRESSED_FILE" -pix_fmt yuv420p "$TEMP_RAW" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Error: Failed to decode $COMPRESSED_FILE" >&2
    rm -f "$TEMP_RAW"
    exit 1
fi

# Calculate SSIM
SSIM_OUTPUT=$(ffmpeg -f rawvideo -s "$RESOLUTION" -pix_fmt yuv420p -i "$TEMP_RAW" \
                    -f rawvideo -s "$RESOLUTION" -pix_fmt yuv420p -i "$REF_FILE" \
                    -lavfi ssim -f null - 2>&1)

# Clean up temp file
rm -f "$TEMP_RAW"

# Extract the "All" SSIM value (format: All:0.XXXXXX)
SSIM_VALUE=$(echo "$SSIM_OUTPUT" | grep -oP 'All:\K[0-9.]+')

if [ -z "$SSIM_VALUE" ]; then
    echo "Error: Could not extract SSIM value" >&2
    echo "FFmpeg output: $SSIM_OUTPUT" >&2
    exit 1
fi

# Output just the numeric value
echo "$SSIM_VALUE"
