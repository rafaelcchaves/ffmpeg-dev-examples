#!/usr/bin/env bash

usage() {
    echo "Usage: $0 -i <input-video> -r <Csv-Result> [-o <output-dir>]"
    exit 1
}

output_dir="."
lz_algorithm=""

while getopts "i:r:o:l:" opt; do
    case "$opt" in
        i) in_file="$OPTARG";;
        r) results_file="$OPTARG";;
        o) output_dir="$OPTARG";;
        l) lz_algorithm="$OPTARG";;
        *) usage;; 
    esac
done

if [ -z "$in_file" ] || [ -z "$results_file" ]; then
    usage
fi

export LD_LIBRARY_PATH="/usr/local/lib"

echo "threads_in,threads_out,type,time" > "$results_file"

mkdir -p "$output_dir"

if [ "$lz_algorithm" == "" ]; then

    for i in 1 2 4 8; do
        echo ">>> Building with THREADS_IN=$i"

        bin="decode_$i"
        gcc -O3 -Wall -Wno-unused-variable -Wno-unused-function decode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm -llz4 -llzo2 -DTHREADS_IN="$i"

        output_path="$output_dir/${i}.yuv"

        echo ">>> Running $bin ..."
        "./$bin" -i "$in_file" -o "$output_path" >> "$results_file"

        rm -f "$bin"
    done

else

    for i in 1; do
        echo ">>> Building with THREADS_IN=$i"

        use_lz_decompress="-DUSE_LZ_DECOMPRESS"

        bin="decode_$i"
        gcc -O3 -Wall -Wno-unused-variable -Wno-unused-function decode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm -llz4 -llzo2 -DTHREADS_IN="$i" $use_lz_decompress

        output_path="$output_dir/${i}.yuv"

        echo ">>> Running $bin ..."
        "./$bin" -i "$in_file" -o "$output_path" >> "$results_file"

        rm -f "$bin"
    done

fi