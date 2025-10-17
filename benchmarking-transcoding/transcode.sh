#!/usr/bin/env bash

usage() {
    echo "Usage: $0 -i <input-video> -s <Width>x<Height> -f <Fps> -o <Csv-Result> -e <encoder> [-d <output-dir>]"
    exit 1
}

output_dir="."

while getopts "i:s:f:o:e:d:" opt; do
    case "$opt" in
        i) in_file="$OPTARG";;
        s) size_arg="$OPTARG";;
        f) fps="$OPTARG";;
        o) out_file="$OPTARG";;
        e) encoder_name="$OPTARG";;
        d) output_dir="$OPTARG";;
        *) usage;; 
    esac
done

if [ -z "$in_file" ] || [ -z "$size_arg" ] || [ -z "$fps" ] || [ -z "$out_file" ] || [ -z "$encoder_name" ]; then
    usage
fi

width=$(echo "$size_arg" | cut -d'x' -f1)
height=$(echo "$size_arg" | cut -d'x' -f2)

export LD_LIBRARY_PATH="/usr/local/lib"

echo "threads_in,threads_out,type,time" > "$out_file"

mkdir -p "$output_dir"

for i in 0 1 4 8 12 16; do
    for j in 0 1 4 8 12 16; do
        echo ">>> Building with THREADS_IN=$i THREADS_OUT=$j"

        bin="transcode_${i}_${j}"
        gcc -O3 -Wall transcode.c -o "$bin"  -I/usr/local/include -L/usr/local/lib \
        -lavcodec -lavutil  -lm -lz -lva -laom -ldav1d -lass -lfreetype -lfdk-aac \
        -lmp3lame -lopus -lvorbisenc -lvorbis -lvpx -lx264 -lx265 -lvmaf \
        -lgnutls -lSvtJpegxs -lswresample -lX11 -lvdpau -lva-x11 -lva-drm -llzma \
            -DTHREADS_IN="$i" -DTHREADS_OUT="$j"

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
        "./$bin" -i "$in_file" -o "$output_path" -s "${width}x${height}" -f "$fps" -e "$encoder_name" >> "$out_file"

        rm -f "$bin"
    done
done
