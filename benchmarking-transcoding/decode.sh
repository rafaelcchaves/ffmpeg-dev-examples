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

echo "profile,threads_in,threads_out,type,time" > "$results_file"

mkdir -p "$output_dir"

if [ "$lz_algorithm" == "" ]; then

    # Define thread profiles: name:threads_in
    declare -a profiles=(
        "low_latency:1"
        "balanced:8"
        "high_throughput:16"
    )

    for profile_config in "${profiles[@]}"; do
        IFS=':' read -r profile_name threads_in_config <<< "$profile_config"

        bin="decode_${profile_name}"
        echo ">>> Building $profile_name with THREADS_IN=$threads_in_config"

        gcc -O3 -Wall -Wno-unused-variable -Wno-unused-function decode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm -llz4 -llzo2 -DTHREADS_IN="$threads_in_config"

        output_path="$output_dir/${profile_name}.yuv"

        echo ">>> Running $bin ..."
        "./$bin" -i "$in_file" -o "$output_path" -p "$profile_name" >> "$results_file"

        rm -f "$bin"
    done

else

    # Para LZ, apenas low_latency com 1 thread
    profile_name="low_latency"
    threads_in_config=1

    bin="decode_${profile_name}"
    echo ">>> Building $profile_name with THREADS_IN=$threads_in_config"

    use_lz_decompress="-DUSE_LZ_DECOMPRESS"

    gcc -O3 -Wall -Wno-unused-variable -Wno-unused-function decode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -llzo2 -DTHREADS_IN="$threads_in_config" $use_lz_decompress

    output_path="$output_dir/${profile_name}.yuv"

    echo ">>> Running $bin ..."
    "./$bin" -i "$in_file" -o "$output_path" -p "$profile_name" >> "$results_file"

    rm -f "$bin"

fi