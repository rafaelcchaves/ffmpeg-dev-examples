#!/bin/bash

# --- Usage ---
if [ "$#" -lt 4 ]; then
    echo "Usage: $0 <width>x<height> <reference_codec> <reference_file> <codec2> <file2> [<codec3> <file3> ...]"
    exit 1
fi

resolution="$1"
shift

# --- Reference File ---
ref_codec="$1"
ref_file="$2"
ref_raw="ref_decoded.yuv"
shift 2

echo "Decoding reference file: $ref_file (codec: $ref_codec)"
ffmpeg -loglevel quiet -threads 4 -f "$ref_codec" -i "$ref_file" -y -c:v rawvideo "$ref_raw"
if [ $? -ne 0 ]; then echo "Error: Failed to decode reference file." >&2; exit 1; fi

# --- Results ---
echo "File Sizes:"
echo "  $ref_codec (compressed): $(ls -lh "$ref_file" | awk '{print $5}')"

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
    ssim=$(ffmpeg -loglevel quiet -f rawvideo -s "$resolution" -i "$raw" -f rawvideo -s "$resolution" -i "$ref_raw" -lavfi ssim -f null - 2>&1 | grep "All:" | awk -F 'All:' '{print $2}' | awk '{print $1}')
    echo "  $ssim"

    rm "$raw"
    i=$((i+1))
done

echo ""
echo "Raw YUV size: $(ls -lh "$ref_raw" | awk '{print $5}')"

# --- Cleanup ---
rm "$ref_raw"
