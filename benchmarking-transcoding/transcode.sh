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
    echo "profile,threads_in,threads_out,type,time,acceleration" > "$results_file"
elif [ "$encoder_name" = "lz4hc" ]; then
    echo "profile,threads_in,threads_out,type,time,compression_level" > "$results_file"
else
    echo "profile,threads_in,threads_out,type,time" > "$results_file"
fi

mkdir -p "$output_dir"

# Define thread profiles: name:threads_in:threads_out
declare -a profiles=(
    "low_latency:1:1"
    "balanced:8:8"
    "high_throughput:16:16"
)

for profile_config in "${profiles[@]}"; do
    IFS=':' read -r profile_name threads_in_config threads_out_config <<< "$profile_config"

    bin="transcode_${profile_name}"
    ext=""
    use_lz_compress=""
    threads_out=$threads_out_config
    lz_config=1

    if [ "$encoder_name" = "mjpeg" ]; then
        ext="mjpeg"
    elif [ "$encoder_name" = "libsvtjpegxs" ]; then
        ext="jpegxs"
    elif [ "$encoder_name" = "lz4" ]; then
        ext="lz4"
        use_lz_compress="-DUSE_LZ_COMPRESS"
        lz_config=$threads_in_config
        threads_out=1 # Não foi implementado LZ4 multi-thread
    elif [ "$encoder_name" = "lz4hc" ]; then
        ext="lz4hc"
        use_lz_compress="-DUSE_LZ_COMPRESS"
        lz_config=$(( $threads_in_config < 12 ? $threads_in_config : 12 )) # LZ4HC suporta no máximo 12
        threads_out=1 # Não foi implementado LZ4HC multi-thread
    else
        ext="out"
    fi

    echo ">>> Building $profile_name with THREADS_IN=$threads_in_config THREADS_OUT=$threads_out"

    gcc -O3 -Wall -Wno-unused-variable transcode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -llzo2 -DTHREADS_IN="$threads_in_config" -DTHREADS_OUT=$threads_out $use_lz_compress -DLZ_CONFIG=$lz_config
    output_path="$output_dir/${profile_name}.$ext"

    echo ">>> Running $bin ..."
    "./$bin" -i "$in_file" -o "$output_path" -e "$encoder_name" -p "$profile_name" >> "$results_file"

    rm -f "$bin"
done
