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

if [ "$encoder_name" = "lz4" ]; then
    echo "threads_in,threads_out,type,time,acceleration" > "$results_file"
elif [ "$encoder_name" = "lz4hc" ]; then
    echo "threads_in,threads_out,type,time,compression_level" > "$results_file"
else
    echo "threads_in,threads_out,type,time" > "$results_file"
fi

mkdir -p "$output_dir"

for i in 1 2 4 8 12 16; do
        echo ">>> Building with THREADS_IN=$i THREADS_OUT=$i"

        bin="transcode_${i}_${i}"
        ext=""
        use_lz_compress=""
        if [ "$encoder_name" = "mjpeg" ]; then
            ext="mjpeg"
        elif [ "$encoder_name" = "libsvtjpegxs" ]; then
            ext="jpegxs"
        elif [ "$encoder_name" = "lz4" ]; then
            ext="lz4"
            use_lz_compress="-DUSE_LZ_COMPRESS"
            lz_config=$i
        elif [ "$encoder_name" = "lz4hc" ]; then
            ext="lz4hc"
            use_lz_compress="-DUSE_LZ_COMPRESS"
            lz_config=$(( $i < 12 ? $i : 12 )) # LZ4HC suporta no máximo 12
        else
            ext="out"
        fi

        gcc -O3 -Wall -Wno-unused-variable transcode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm -llz4 -llzo2 -DTHREADS_IN="$i" -DTHREADS_OUT="$i" $use_lz_compress -DLZ_CONFIG=$lz_config
        output_path="$output_dir/${i}_${j}.$ext"

        echo ">>> Running $bin ..."
        "./$bin" -i "$in_file" -o "$output_path" -e "$encoder_name" >> "$results_file"

        rm -f "$bin"
done
