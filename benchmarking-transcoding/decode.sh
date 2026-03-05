#!/usr/bin/env bash

usage() {
    echo "Usage: $0 -i <input-video> -r <csv-result> [-o <output-dir>] [-l <lz-algorithm>] [-p <profile>]"
    echo "  -i  Input video file"
    echo "  -r  CSV result file"
    echo "  -o  Output directory (default: .)"
    echo "  -l  LZ algorithm (lz4 or lz4hc)"
    echo "  -p  Profile: low_latency, balanced, high_throughput (default: all profiles)"
    exit 1
}

output_dir="."
lz_algorithm=""
profile=""

while getopts "i:r:o:l:p:" opt; do
    case "$opt" in
        i) in_file="$OPTARG";;
        r) results_file="$OPTARG";;
        o) output_dir="$OPTARG";;
        l) lz_algorithm="$OPTARG";;
        p) profile="$OPTARG";;
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
        "balanced:4"
        "high_throughput:8"
    )

    for profile_config in "${profiles[@]}"; do
        IFS=':' read -r profile_name threads_in_config <<< "$profile_config"

        bin="decode_${profile_name}"
        echo ">>> Building $profile_name with THREADS_IN=$threads_in_config"

        g++ -O3 -Wall -Wno-unused-variable -Wno-unused-function src/decode.cpp src/cpu_stats.cpp -o "$bin" -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm

        output_path="$output_dir/${profile_name}.yuv"

        echo ">>> Running $bin ..."
        "./$bin" -i "$in_file" -o "$output_path" -p "$profile_name" >> "$results_file"

        rm -f "$bin"
    done

else

    # Para LZ, define profiles: name:threads_in
    declare -a lz_profiles=(
        "low_latency:1"
        "balanced:4"
        "high_throughput:8"
    )

    # Se profile especificado, filtra apenas ele
    if [ "$profile" != "" ]; then
        filtered_profiles=()
        for p in "${lz_profiles[@]}"; do
            IFS=':' read -r pname pthreads <<< "$p"
            if [ "$pname" == "$profile" ]; then
                filtered_profiles+=("$p")
            fi
        done
        lz_profiles=("${filtered_profiles[@]}")
    fi

    for profile_config in "${lz_profiles[@]}"; do
        IFS=':' read -r profile_name threads_in_config <<< "$profile_config"

        bin="decode_${profile_name}"
        echo ">>> Building $profile_name with THREADS_IN=$threads_in_config"

        g++ -O3 -Wall -Wno-unused-variable -Wno-unused-function \
            src/decode_lz4/decode_lz4_main.cpp src/decode_lz4/decode_lz4_mt.cpp \
            src/decode_lz4/frame_reader.cpp src/decode_lz4/frame_decoder.cpp \
            src/decode_lz4/frame_writer.cpp src/decode_lz4/stats.cpp \
            src/cpu_stats.cpp \
            -o "$bin" -I/usr/local/include -L/usr/local/lib \
            -lavutil -lm -llz4 -lpthread

        output_path="$output_dir/${profile_name}.yuv"

        echo ">>> Running: ./$bin -i $in_file -o $output_path -p $profile_name -t $threads_in_config"
        "./$bin" -i "$in_file" -o "$output_path" -p "$profile_name" -t "$threads_in_config" >> "$results_file" 2>&1

        rm -f "$bin"
    done

fi
