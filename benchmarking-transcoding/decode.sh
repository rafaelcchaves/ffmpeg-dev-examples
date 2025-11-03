#!/usr/bin/env bash

usage() {
    echo "Usage: $0 -i <input-video> -r <Csv-Result> [-o <output-dir>]"
    exit 1
}

output_dir="."

while getopts "i:r:o:" opt; do
    case "$opt" in
        i) in_file="$OPTARG";;
        r) results_file="$OPTARG";;
        o) output_dir="$OPTARG";;
        *) usage;; 
    esac
done

if [ -z "$in_file" ] || [ -z "$results_file" ]; then
    usage
fi

export LD_LIBRARY_PATH="/usr/local/lib"

echo "threads_in,threads_out,type,time" > "$results_file"

mkdir -p "$output_dir"

for i in 1 2 4 8 12 16; do
    echo ">>> Building with THREADS_IN=$i"

    bin="decode_$i"
    gcc -O3 -Wall decode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -DTHREADS_IN="$i"

    output_path="$output_dir/${i}.yuv"

    echo ">>> Running $bin ..."
    "./$bin" -i "$in_file" -o "$output_path" >> "$results_file"

    rm -f "$bin"
done
