#!/bin/bash

# --- Configuration ---
# Set the name of your raw input file
INPUT_FILE=""
OUT_SIZE="3840x2160" # Default output size

usage() {
    echo "Usage: $0 -i <input_file_path> [-s <output_size>] [-e <encoder>]"
    echo "  -i    Path to the raw input file (e.g., original.yuv)"
    echo "  -s    Output video resolution (e.g., 1920x1080). Defaults to 3840x2160."
    echo "  -e    Encoder to use: mjpeg, vvc, av1, vp9, avc, hevc, jpegxs. If omitted, runs all."
    exit 1
}

while getopts "i:s:e:h" opt; do
    case "$opt" in
        i) INPUT_FILE="$OPTARG";;
        s) OUT_SIZE="$OPTARG";;
        e) ENCODER="$OPTARG";;
        h) usage;;
        *) usage;;
    esac
done

# Validate encoder if specified
VALID_ENCODERS=("mjpeg" "vvc" "av1" "vp9" "avc" "hevc" "jpegxs")
if [[ -n "$ENCODER" ]]; then
    if [[ ! " ${VALID_ENCODERS[@]} " =~ " ${ENCODER} " ]]; then
        echo "Error: Invalid encoder '$ENCODER'. Valid options: ${VALID_ENCODERS[*]}" >&2
        exit 1
    fi
fi
shift $((OPTIND-1))

if [[ -z "$INPUT_FILE" ]]; then
    echo "Error: Input file not specified." >&2
    usage
fi

# Input parameters
IN_SIZE="3840x2160" # This will be overridden by OUT_SIZE in ffmpeg command if different
IN_PIX_FMT="yuv420p"
IN_FPS="50"

# Output parameters
OUT_FPS="50"

# --- End of Configuration ---

# This combines all input flags into one variable for readability
INPUT_FLAGS="-f rawvideo -r $IN_FPS -pixel_format $IN_PIX_FMT -video_size $IN_SIZE -i $INPUT_FILE"

# --- Start Encoding ---
# FFmpeg will process these jobs one by one.

run_encode() {
    local enc=$1
    case "$enc" in
        mjpeg)
            echo "-----------------------------------"
            echo "Starting MJPEG (mjpeg) encode... (Quality 10)"
            ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v mjpeg -q:v 10 -r $OUT_FPS ${OUT_SIZE}_mjpeg.mp4
            ;;
        vvc)
            echo "-----------------------------------"
            echo "Starting VVC (vvenc) encode... (Preset medium)"
            ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libvvenc -preset medium -r $OUT_FPS ${OUT_SIZE}_vvc.mp4
            ;;
        av1)
            echo "-----------------------------------"
            echo "Starting AV1 (libsvtav1) encode... (Preset 6)"
            ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libsvtav1 -preset 6 -r $OUT_FPS ${OUT_SIZE}_av1.mp4
            ;;
        vp9)
            echo "-----------------------------------"
            echo "Starting VP9 (libvpx-vp9) encode... (CPU-Used 4)"
            # -b:v 0 is required for CRF mode in libvpx-vp9
            ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libvpx-vp9 -cpu-used 4 -b:v 0 -r $OUT_FPS ${OUT_SIZE}_vp9.mp4
            ;;
        avc)
            echo "-----------------------------------"
            echo "Starting AVC (libx264) encode... (Preset medium)"
            ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libx264 -preset medium -r $OUT_FPS ${OUT_SIZE}_avc.mp4
            ;;
        hevc)
            echo "-----------------------------------"
            echo "Starting HEVC (libx265) encode... (Preset medium)"
            ffmpeg $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libx265 -preset medium -r $OUT_FPS ${OUT_SIZE}_hevc.mp4
            ;;
        jpegxs)
            echo "-----------------------------------"
            echo "Starting JPEG XS (libsvtjpegxs) encode..."
            ffmpeg -y $INPUT_FLAGS -vf scale=$OUT_SIZE -c:v libsvtjpegxs -bpp 8 -r $OUT_FPS ${OUT_SIZE}_jpegxs.mp4
            ;;
    esac
}

# Run encoding based on encoder selection
if [[ -n "$ENCODER" ]]; then
    run_encode "$ENCODER"
else
    for enc in "${VALID_ENCODERS[@]}"; do
        run_encode "$enc"
    done
fi

echo "-----------------------------------"
echo "All encoding jobs are complete."
