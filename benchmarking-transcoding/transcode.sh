#!/usr/bin/env bash

usage() {
    echo "Usage: $0 -i <input-video> -r <csv-result> -e <encoder> [-o <output-dir>]"
    echo "  -i  Input video file"
    echo "  -r  CSV result file"
    echo "  -e  Encoder (mjpeg, libsvtjpegxs, lz4, lz4hc, or other)"
    echo "  -o  Output directory (default: .)"
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
    threads_out=$threads_out_config
    lz_config=1

    # Determina extensão e configurações específicas do encoder
    if [ "$encoder_name" = "mjpeg" ]; then
        ext="mjpeg"
    elif [ "$encoder_name" = "libsvtjpegxs" ]; then
        ext="jpegxs"
    elif [ "$encoder_name" = "lz4" ]; then
        ext="lz4"
        lz_config=1
    elif [ "$encoder_name" = "lz4hc" ]; then
        ext="lz4hc"
        lz_config=$(( threads_in_config < 12 ? threads_in_config : 12 )) # LZ4HC suporta no máximo 12
    else
        ext="out"
    fi

    output_path="$output_dir/${profile_name}.$ext"

    # =========================================================================
    # Escolha entre single-thread (transcode.c) e multithread (transcode_lz4_mt)
    # =========================================================================

    if [ "$encoder_name" = "lz4" ] || [ "$encoder_name" = "lz4hc" ]; then
        # LZ4/LZ4HC: usar multithread se threads_out > 1, senão single-thread
        if [ "$threads_out" -gt 1 ]; then
            # Multithread LZ4/LZ4HC
            echo ">>> Building $profile_name (multithread) with THREADS_IN=$threads_in_config THREADS_OUT=$threads_out"

            gcc -O3 -Wall -Wno-unused-variable \
                -DTHREADS_IN="$threads_in_config" -DTHREADS_OUT="$threads_out" \
                src/transcode_lz4/transcode_lz4_main.c \
                src/transcode_lz4/transcode_lz4_mt.c \
                src/avbuffer_queue.c \
                src/cpu_stats.cpp \
                -o "$bin" \
                -I/usr/local/include -L/usr/local/lib \
                -lavcodec -lavutil -lavformat -lm -llz4 -lpthread

            echo ">>> Running $bin (multithread) ..."
            "./$bin" -i "$in_file" -o "$output_path" -e "$encoder_name" -l "$lz_config" -p "$profile_name" >> "$results_file"
        else
            # Single-thread LZ4/LZ4HC (usando transcode.c)
            echo ">>> Building $profile_name (single-thread) with THREADS_IN=$threads_in_config THREADS_OUT=$threads_out"

            gcc -O3 -Wall -Wno-unused-variable src/transcode.c -o "$bin" -I/usr/local/include -L/usr/local/lib \
                -lavcodec -lavutil -lavformat -lm -llz4 -llzo2 \
                -DTHREADS_IN="$threads_in_config" -DTHREADS_OUT=$threads_out \
                -DUSE_LZ_COMPRESS -DLZ_CONFIG=$lz_config

            echo ">>> Running $bin (single-thread) ..."
            "./$bin" -i "$in_file" -o "$output_path" -e "$encoder_name" -p "$profile_name" >> "$results_file"
        fi
    else
        # Outros encoders (mjpeg, libsvtjpegxs, etc.) - sempre usar transcode.c
        echo ">>> Building $profile_name with THREADS_IN=$threads_in_config THREADS_OUT=$threads_out"

        gcc -O3 -Wall -Wno-unused-variable src/transcode.c -o "$bin" -I/usr/local/include -L/usr/local/lib \
            -lavcodec -lavutil -lavformat -lm -llz4 -llzo2 \
            -DTHREADS_IN="$threads_in_config" -DTHREADS_OUT=$threads_out

        echo ">>> Running $bin ..."
        "./$bin" -i "$in_file" -o "$output_path" -e "$encoder_name" -p "$profile_name" >> "$results_file"
    fi

    rm -f "$bin"
done
