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
    ffmpeg -loglevel quiet -i "$file" "$raw"
    if [ $? -ne 0 ]; then echo "Error: Failed to decode comparison file." >&2; exit 1; fi

    echo "  $codec (compressed): $(ls -lh "$file" | awk '{print $5}')"

    echo "SSIM ($codec vs $ref_file):"
    cmd="ffmpeg -f rawvideo -s $resolution -i $raw -f rawvideo -s $resolution -i $ref_file -lavfi ssim -f null -"
    $cmd &> /tmp/ssim_output
    status=$?
    ssim=$(cat /tmp/ssim_output)
    if [ $status -ne 0 ]; then
        printf "%s\n" "$cmd"
        grep Error /tmp/ssim_output
        rm -f "$raw"
        exit 1
    fi
    grep SSIM /tmp/ssim_output
    rm -f /tmp/ssim_output
    rm $raw
    i=$((i+1))
done
