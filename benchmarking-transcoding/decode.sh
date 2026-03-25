#!/usr/bin/env bash

usage() {
    echo "Usage: $0 -i <input-video> -r <csv-result> [-o <output-dir>] [-l <lz-algorithm>] [-p <profile>] [-w]"
    echo "  -i  Input video file"
    echo "  -r  CSV result file"
    echo "  -o  Output directory (default: .)"
    echo "  -l  LZ algorithm (lz4 or lz4hc)"
    echo "  -p  Profile: low_latency, balanced, high_throughput (default: all profiles)"
    echo "  -w  Enable output file writing (default: disabled for benchmark)"
    exit 1
}

output_dir="."
lz_algorithm=""
profile=""
enable_write=0

while getopts "i:r:o:l:p:w" opt; do
    case "$opt" in
        i) in_file="$OPTARG";;
        r) results_file="$OPTARG";;
        o) output_dir="$OPTARG";;
        l) lz_algorithm="$OPTARG";;
        p) profile="$OPTARG";;
        w) enable_write=1;;
        *) usage;;
    esac
done

if [ -z "$in_file" ] || [ -z "$results_file" ]; then
    usage
fi

export LD_LIBRARY_PATH="/usr/local/lib"

echo "profile,decoder_threads,encoder_threads,type,time" > "$results_file"

mkdir -p "$output_dir"

# Define WRITE_FLAG based on enable_write option
if [ "$enable_write" -eq 1 ]; then
    WRITE_FLAG="-DENABLE_OUTPUT_WRITE=1"
else
    WRITE_FLAG=""
fi

if [ "$lz_algorithm" == "" ]; then

    # Define thread profiles: name:decoder_threads
    declare -a profiles=(
        "low_latency:1"
        "balanced:4"
        "high_throughput:8"
    )

    # =========================================================================
    # Compile once
    # =========================================================================

    echo ">>> Building decode ..."
    g++ -O3 -Wall -Wno-unused-variable -Wno-unused-function \
        src/decode.cpp src/cpu_stats.cpp -o decode -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm $WRITE_FLAG

    for profile_config in "${profiles[@]}"; do
        IFS=':' read -r profile_name decoder_threads_config <<< "$profile_config"

        output_path="$output_dir/${profile_name}.yuv"

        echo ">>> Running decode for $profile_name with -D $decoder_threads_config ..."
        "./decode" -i "$in_file" -o "$output_path" -p "$profile_name" -D "$decoder_threads_config" >> "$results_file"
    done

    rm -f decode

else

    # Para LZ, define profiles: name:decoder_threads
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

    # =========================================================================
    # Compile once
    # =========================================================================

    echo ">>> Building decode_lz4 ..."
    g++ -O3 -Wall -Wno-unused-variable -Wno-unused-function \
        $WRITE_FLAG \
        src/decode_lz4/decode_lz4_main.cpp src/decode_lz4/decode_lz4_mt.cpp \
        src/decode_lz4/frame_reader.cpp src/decode_lz4/frame_decoder.cpp \
        src/decode_lz4/frame_writer.cpp src/decode_lz4/stats.cpp \
        src/queue.c src/cpu_stats.cpp \
        -o decode_lz4 -I/usr/local/include -L/usr/local/lib \
        -lavutil -lm -llz4 -lpthread

    for profile_config in "${lz_profiles[@]}"; do
        IFS=':' read -r profile_name decoder_threads_config <<< "$profile_config"

        output_path="$output_dir/${profile_name}.yuv"

        echo ">>> Running decode_lz4 for $profile_name with -D $decoder_threads_config ..."
        "./decode_lz4" -i "$in_file" -o "$output_path" -p "$profile_name" -D "$decoder_threads_config" >> "$results_file" 2>&1
    done

    rm -f decode_lz4

fi
