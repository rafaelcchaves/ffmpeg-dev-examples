#!/usr/bin/env bash

if [[ $# -ne 5 ]]; then
    echo "Usage: $0 <input-video> <Width> <Height> <Fps> <Csv-Result>"
    exit 1
fi

in_file="$1"
width="$2"
height="$3"
fps="$4"
out_file="$5"

echo "threads_in,threads_out,type,time" > "$out_file"

for i in 0 1 4 8 12 16; do
    for j in 0 1 4 8 12 16; do
        echo ">>> Building with THREADS_IN=$i THREADS_OUT=$j"

        bin="transcode_${i}_${j}"
        gcc -O3 -Wall transcode.c -o "$bin" -lavcodec -lavutil \
            -DTHREADS_IN="$i" -DTHREADS_OUT="$j"

        echo ">>> Running $bin ..."
        "./$bin" $in_file "${i}_${j}.mjpeg" $width $height $fps >> "$out_file"

        rm -f "$bin"
    done
done
