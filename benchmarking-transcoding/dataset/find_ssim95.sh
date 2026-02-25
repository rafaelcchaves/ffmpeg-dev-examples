#!/bin/bash

# --- Binary Search for SSIM ≈ 0.95 ---
# Finds the quality parameter that achieves SSIM between 0.945 and 0.955
# for each supported codec.

# --- Configuration ---
RESOLUTION="3840x2160"
INPUT_FILE="original.yuv"
TARGET_SSIM=0.95
TOLERANCE=0.005
MIN_SSIM=$(echo "$TARGET_SSIM - $TOLERANCE" | bc)
MAX_SSIM=$(echo "$TARGET_SSIM + $TOLERANCE" | bc)
MAX_ITERATIONS=10
OUTPUT_DIR="."
RESULTS_FILE="ssim95_results.csv"

# --- Usage ---
usage() {
    echo "Usage: $0 [-c <codec>] [-i <input_file>] [-o <output_dir>]"
    echo "  -c    Codec to search: mjpeg, avc, hevc, vp9, jpegxs (default: all)"
    echo "  -i    Input YUV file (default: original.yuv)"
    echo "  -o    Output directory (default: current dir)"
    echo "  -h    Show this help"
    exit 1
}

# --- Parse Arguments ---
CODEC=""
while getopts "c:i:o:h" opt; do
    case "$opt" in
        c) CODEC="$OPTARG" ;;
        i) INPUT_FILE="$OPTARG" ;;
        o) OUTPUT_DIR="$OPTARG" ;;
        h) usage ;;
        *) usage ;;
    esac
done

# --- Validate Input ---
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file '$INPUT_FILE' not found" >&2
    exit 1
fi

# --- Codec Configuration ---
# Format: param_name|min|max|direction
# direction: "normal" = higher param = lower quality (CRF/q:v)
#            "inverse" = higher param = higher quality (bpp)
declare -A CODEC_CONFIG
CODEC_CONFIG["mjpeg"]="q:v|1|31|normal"
CODEC_CONFIG["avc"]="crf|0|51|normal"
CODEC_CONFIG["hevc"]="crf|0|51|normal"
CODEC_CONFIG["vp9"]="crf|0|63|normal"
CODEC_CONFIG["jpegxs"]="bpp|1|12|inverse"

# --- Encoding Functions ---
get_input_flags() {
    echo "-f rawvideo -r 50 -pixel_format yuv420p -video_size 3840x2160 -i $INPUT_FILE"
}

encode_mjpeg() {
    local value=$1
    local output=$2
    ffmpeg -loglevel warning $(get_input_flags) -c:v mjpeg -q:v $value -r 50 "$output"
}

encode_avc() {
    local value=$1
    local output=$2
    ffmpeg -loglevel warning $(get_input_flags) -c:v libx264 -preset medium -crf $value -r 50 "$output"
}

encode_hevc() {
    local value=$1
    local output=$2
    ffmpeg -loglevel warning $(get_input_flags) -c:v libx265 -preset medium -crf $value -r 50 "$output"
}

encode_vp9() {
    local value=$1
    local output=$2
    # VP9 requires -b:v 0 for CRF mode
    ffmpeg -loglevel warning $(get_input_flags) -c:v libvpx-vp9 -cpu-used 4 -crf $value -b:v 0 -r 50 "$output"
}

encode_jpegxs() {
    local value=$1
    local output=$2
    ffmpeg -loglevel warning -y $(get_input_flags) -c:v libsvtjpegxs -bpp $value -r 50 "$output"
}

# --- SSIM Calculation ---
get_ssim() {
    local compressed=$1
    local temp_raw="/tmp/find_ssim_$$_decoded.yuv"

    ffmpeg -loglevel quiet -i "$compressed" -pix_fmt yuv420p "$temp_raw" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "-1"
        return 1
    fi

    local ssim_output=$(ffmpeg -f rawvideo -s "$RESOLUTION" -pix_fmt yuv420p -i "$temp_raw" \
                              -f rawvideo -s "$RESOLUTION" -pix_fmt yuv420p -i "$INPUT_FILE" \
                              -lavfi ssim -f null - 2>&1)

    rm -f "$temp_raw"

    local ssim_value=$(echo "$ssim_output" | grep -oP 'All:\K[0-9.]+')
    echo "${ssim_value:-0}"
}

# --- Binary Search ---
binary_search() {
    local codec=$1
    local param_name=$2
    local min_val=$3
    local max_val=$4
    local direction=$5

    local output_file="$OUTPUT_DIR/ssim95_${codec}.mp4"
    local test_file="/tmp/find_ssim_$$_test_${codec}.mp4"

    echo "========================================="
    echo "Searching for $codec: SSIM target = $TARGET_SSIM (±$TOLERANCE)"
    echo "Parameter: $param_name, Range: [$min_val, $max_val], Direction: $direction"
    echo "========================================="

    local low=$min_val
    local high=$max_val
    local iteration=0
    local found_value=""
    local found_ssim=""

    while [ $iteration -lt $MAX_ITERATIONS ]; do
        iteration=$((iteration + 1))

        # Calculate midpoint (use integer arithmetic for parameters)
        local mid=$(( (low + high) / 2 ))

        echo ""
        echo "--- Iteration $iteration: $param_name = $mid ---"

        # Encode with current value
        echo "Encoding with $param_name=$mid..."
        rm -f "$test_file"
        encode_${codec} "$mid" "$test_file"

        if [ $? -ne 0 ] || [ ! -f "$test_file" ]; then
            echo "Error: Encoding failed for $codec with $param_name=$mid"
            # Adjust range and continue
            if [ "$direction" = "normal" ]; then
                low=$((mid + 1))
            else
                high=$((mid - 1))
            fi
            continue
        fi

        # Calculate SSIM
        echo "Calculating SSIM..."
        local ssim=$(get_ssim "$test_file")
        echo "SSIM = $ssim"

        # Check if within target range
        local in_range=$(echo "$ssim >= $MIN_SSIM && $ssim <= $MAX_SSIM" | bc -l)

        if [ "$in_range" = "1" ]; then
            echo "✓ FOUND! SSIM $ssim is within target range [$MIN_SSIM, $MAX_SSIM]"
            found_value=$mid
            found_ssim=$ssim

            # Copy to final output
            cp "$test_file" "$output_file"
            rm -f "$test_file"
            break
        fi

        # Adjust search range based on direction and SSIM
        local too_high=$(echo "$ssim > $MAX_SSIM" | bc -l)

        if [ "$direction" = "normal" ]; then
            # Higher param = lower quality = lower SSIM
            if [ "$too_high" = "1" ]; then
                echo "SSIM too high, need more compression (increase $param_name)"
                low=$((mid + 1))
            else
                echo "SSIM too low, need less compression (decrease $param_name)"
                high=$((mid - 1))
            fi
        else
            # Inverse: Higher param = higher quality = higher SSIM (JPEG XS)
            if [ "$too_high" = "1" ]; then
                echo "SSIM too high, need more compression (decrease $param_name)"
                high=$((mid - 1))
            else
                echo "SSIM too low, need less compression (increase $param_name)"
                low=$((mid + 1))
            fi
        fi

        # Check if search space exhausted
        if [ $low -gt $high ]; then
            echo "Search space exhausted. Using closest value found."
            # Use the current test file as best effort
            found_value=$mid
            found_ssim=$ssim
            cp "$test_file" "$output_file"
            break
        fi
    done

    rm -f "$test_file"

    if [ -n "$found_value" ]; then
        echo ""
        echo "========================================="
        echo "Result for $codec:"
        echo "  Parameter: $param_name = $found_value"
        echo "  SSIM: $found_ssim"
        echo "  Output: $output_file"
        echo "========================================="

        # Get file size
        local file_size=$(ls -lh "$output_file" | awk '{print $5}')
        local file_bytes=$(stat -c%s "$output_file" 2>/dev/null || stat -f%z "$output_file" 2>/dev/null)

        # Calculate compression ratio (original / compressed)
        local orig_bytes=$(stat -c%s "$INPUT_FILE" 2>/dev/null || stat -f%z "$INPUT_FILE" 2>/dev/null)
        local ratio=$(echo "scale=0; $orig_bytes / $file_bytes" | bc)

        # Return results as a pipe-separated string
        echo "${codec}|${param_name}|${found_value}|${found_ssim}|${file_size}|${ratio}:1"
    else
        echo "Failed to find suitable parameter for $codec"
        echo ""
    fi
}

# --- Main Execution ---
main() {
    # Create output directory if needed
    mkdir -p "$OUTPUT_DIR"

    # Initialize results file
    echo "codec,parameter,value,ssim,file_size,compression_ratio" > "$OUTPUT_DIR/$RESULTS_FILE"

    # Determine which codecs to process
    if [ -n "$CODEC" ]; then
        codecs=("$CODEC")
    else
        codecs=("mjpeg" "avc" "hevc" "vp9" "jpegxs")
    fi

    for codec in "${codecs[@]}"; do
        if [ -z "${CODEC_CONFIG[$codec]}" ]; then
            echo "Error: Unknown codec '$codec'" >&2
            echo "Valid codecs: mjpeg, avc, hevc, vp9, jpegxs" >&2
            exit 1
        fi

        # Parse codec config
        IFS='|' read -r param_name min_val max_val direction <<< "${CODEC_CONFIG[$codec]}"

        # Run binary search
        result=$(binary_search "$codec" "$param_name" "$min_val" "$max_val" "$direction")

        # Parse and append to CSV
        if [ -n "$result" ]; then
            # Convert pipe-separated to comma-separated for CSV
            csv_line=$(echo "$result" | tr '|' ',')
            echo "$csv_line" >> "$OUTPUT_DIR/$RESULTS_FILE"
        fi
    done

    echo ""
    echo "========================================="
    echo "All searches complete!"
    echo "Results saved to: $OUTPUT_DIR/$RESULTS_FILE"
    echo "========================================="
    cat "$OUTPUT_DIR/$RESULTS_FILE"
}

main
