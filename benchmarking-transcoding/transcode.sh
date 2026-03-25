#!/usr/bin/env bash

usage() {
    echo "Usage: $0 -i <input-video> -r <csv-result> -e <encoder> [-o <output-dir>] [-f <fps>] [-w]"
    echo "  -i  Input video file"
    echo "  -r  CSV result file"
    echo "  -e  Encoder (mjpeg, libsvtjpegxs, lz4, lz4hc, or other)"
    echo "  -o  Output directory (default: .)"
    echo "  -f  Target FPS for input read throttling (default: no limit)"
    echo "  -w  Enable output file writing (default: disabled for benchmark)"
    exit 1
}

output_dir="."
enable_write=0
target_fps=""

while getopts "i:r:e:o:f:w" opt; do
    case "$opt" in
        i) in_file="$OPTARG";;
        r) results_file="$OPTARG";;
        e) encoder_name="$OPTARG";;
        o) output_dir="$OPTARG";;
        f) target_fps="$OPTARG";;
        w) enable_write=1;;
        *) usage;;
    esac
done

if [ -z "$in_file" ] || [ -z "$results_file" ] || [ -z "$encoder_name" ]; then
    usage
fi



export LD_LIBRARY_PATH="/usr/local/lib"

if [ "$encoder_name" = "lz4" ]; then
    echo "profile,decoder_threads,encoder_threads,type,time,acceleration" > "$results_file"
elif [ "$encoder_name" = "lz4hc" ]; then
    echo "profile,decoder_threads,encoder_threads,type,time,compression_level" > "$results_file"
else
    echo "profile,decoder_threads,encoder_threads,type,time" > "$results_file"
fi

mkdir -p "$output_dir"

# Define thread profiles: name:decoder_threads:encoder_threads
declare -a profiles=(
    "low_latency:1:1"
    "balanced:8:8"
    "high_throughput:16:16"
)

# =========================================================================
# Compile once based on encoder type
# =========================================================================

if [ "$encoder_name" = "lz4" ] || [ "$encoder_name" = "lz4hc" ]; then
    # LZ4/LZ4HC: always use multithread binary
    echo ">>> Building transcode_lz4 (ST + MT) ..."
    gcc -O3 -Wall -Wno-unused-variable \
        src/transcode_lz4/transcode_lz4_main.c \
        src/transcode_lz4/transcode_lz4_st.c \
        src/transcode_lz4/transcode_lz4_mt.c \
        src/queue.c \
        src/cpu_stats.cpp \
        src/fps_limiter.c \
        -o transcode_mt \
        -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm -llz4 -lpthread
    bin="transcode_mt"
else
    # Other encoders (mjpeg, libsvtjpegxs, etc.) - use transcode.c
    echo ">>> Building transcode ..."
    gcc -O3 -Wall -Wno-unused-variable src/transcode.c src/fps_limiter.c -o transcode -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil -lavformat -lm
    bin="transcode"
fi

for profile_config in "${profiles[@]}"; do
    IFS=':' read -r profile_name decoder_threads_config encoder_threads_config <<< "$profile_config"

    ext=""
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
        lz_config=$(( decoder_threads_config < 12 ? decoder_threads_config : 12 )) # LZ4HC suporta no máximo 12
    else
        ext="out"
    fi

    output_path="$output_dir/${profile_name}.$ext"

    echo ">>> Running $bin for $profile_name with -D $decoder_threads_config -E $encoder_threads_config ..."
    write_flag=""
    if [ "$enable_write" -eq 1 ]; then write_flag="-w"; fi
    fps_flag=""
    if [ -n "$target_fps" ]; then fps_flag="-f $target_fps"; fi
    lz_flag=""
    if [ "$bin" = "transcode_mt" ]; then lz_flag="-l $lz_config"; fi
    "./$bin" -i "$in_file" -o "$output_path" -e "$encoder_name" $lz_flag -p "$profile_name" -D "$decoder_threads_config" -E "$encoder_threads_config" $fps_flag $write_flag >> "$results_file"
done

rm -f "$bin"
