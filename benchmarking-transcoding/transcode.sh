#!/usr/bin/env bash

usage() {
    echo "Usage: $0 -i <input-video> -r <Csv-Result> -e <encoder> [-o <output-dir>]"
    exit 1
}

output_dir="."

while getopts "i:r:e:o:" opt; do
    case "$opt" in
        i) in_file="$OPTARG";;
        r) results_file="$OPTARG";;
        e) encoder_name="$OPTARG";;
        o) output_dir="$OPTARG";;
        *) usage;;
    esac
done

if [ -z "$in_file" ] || [ -z "$results_file" ] || [ -z "$encoder_name" ]; then
    usage
fi



export LD_LIBRARY_PATH="/usr/local/lib"

echo "threads_in,threads_out,type,time" > "$results_file"

mkdir -p "$output_dir"

for i in 1 2 4 8 12 16; do
        echo ">>> Building with THREADS_IN=$i THREADS_OUT=$i"

        bin="transcode_${i}_${i}"
        gcc -O3 -Wall transcode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm -DTHREADS_IN="$i" -DTHREADS_OUT="$i"
        ext=""
        if [ "$encoder_name" = "mjpeg" ]; then
            ext="mjpeg"
        elif [ "$encoder_name" = "libsvtjpegxs" ]; then
            ext="jpegxs"
        else
            ext="out"
        fi

        output_path="$output_dir/${i}_${j}.$ext"

        echo ">>> Running $bin ..."
        "./$bin" -i "$in_file" -o "$output_path" -e "$encoder_name" >> "$results_file"

        rm -f "$bin"
done
