#!/bin/bash

# --- Usage ---
if [ "$#" -lt 4 ]; then
    echo "Usage: $0 <width>x<height> <reference_file> <codec2> <file2> [<codec3> <file3> ...]"
    exit 1
fi

resolution="$1"
shift

# --- Reference File ---
ref_file="$1"
shift 

# --- Results ---
echo "File Sizes:"
echo "   raw: $(ls -lh "$ref_file" | awk '{print $5}')"

# --- Comparison Files ---
i=1
while [ "$#" -gt 0 ]; do
    codec="$1"
    file="$2"
    raw="decoded_$i.yuv"
    shift 2

    echo "Decoding comparison file: $file (codec: $codec)"
    ffmpeg -loglevel quiet -threads 4 -f "$codec" -i "$file" -y -c:v rawvideo "$raw"
    if [ $? -ne 0 ]; then echo "Error: Failed to decode comparison file." >&2; exit 1; fi

    echo "  $codec (compressed): $(ls -lh "$file" | awk '{print $5}')"

    echo "SSIM ($codec vs $ref_codec):"
    ssim=$(ffmpeg -loglevel quiet -f rawvideo -s "$resolution" -i "$raw" -f rawvideo -s "$resolution" -i "$ref_file" -lavfi ssim -f null - 2>&1 | grep "All:" | awk -F 'All:' '{print $2}' | awk '{print $1}')
    echo "  $ssim"

    rm "$raw"
    i=$((i+1))
done
