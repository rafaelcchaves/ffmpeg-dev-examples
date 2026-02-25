---
name: video-generate
description: Comandos FFmpeg para converter vídeos brutos (.yuv) para codecs modernos. Use quando precisar codificar vídeos em AV1, AVC, HEVC, VVC, VP9, MJPEG ou JPEGXS com controle de qualidade.
---

# Conversão de Vídeo YUV para Codecs Modernos

## Parâmetros de Entrada Padrão
- Pixel format: yuv420p
- FPS: 50
- Input flags: `-f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv`

## Codecs Disponíveis

### MJPEG (Motion JPEG)
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -vf scale=WoutxHout -c:v mjpeg -q:v 10 -r 50 output_mjpeg.mp4
```

- **-q:v**: Qualidade (1-31, menor = melhor). Padrão: 10

### AVC / H.264 (libx264)
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -vf scale=WoutxHout -c:v libx264 -preset medium -r 50 output_avc.mp4
```

- **-preset**: ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow
- **-crf**: 0-51 (default 23, menor = melhor qualidade)

### HEVC / H.265 (libx265)
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -vf scale=WoutxHout -c:v libx265 -preset medium -r 50 output_hevc.mp4
```

- **-preset**: ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow
- **-crf**: 0-51 (default 28, menor = melhor qualidade)

### AV1 (libsvtav1)
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -vf scale=WoutxHout -c:v libsvtav1 -preset 6 -r 50 output_av1.mp4
```

- **-preset**: 0-13 (0 = melhor qualidade/lento, 13 = pior qualidade/rápido)

### VP9 (libvpx-vp9)
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -vf scale=WoutxHout -c:v libvpx-vp9 -cpu-used 4 -b:v 0 -r 50 output_vp9.mp4
```

- **-cpu-used**: 0-5 (0 = lento/melhor, 5 = rápido/pior)
- **-crf**: Requer -b:v 0 para modo CRF
- **-crf**: 0-63 (default 31, menor = melhor)

### VVC (libvvenc)
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -vf scale=WoutxHout -c:v libvvenc -preset medium -r 50 output_vvc.mp4
```

- **-preset**: faster, fast, medium, slow, slower

### JPEG XS (libsvtjpegxs)
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -vf scale=WoutxHout -c:v libsvtjpegxs -bpp 4 -r 50 output_jpegxs.mp4
```

- **-bpp**: Bits per pixel (1-12, maior = melhor qualidade)

## Exemplos de Uso

### Converter 4K para 1080p em AV1 com melhor qualidade
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size 3840x2160 -i original.yuv \
  -vf scale=1920x1080 -c:v libsvtav1 -preset 2 -r 50 output_av1.mp4
```

### HEVC com compressão agressiva
```bash
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size 1920x1080 -i original.yuv \
  -c:v libx265 -preset slow -crf 30 -r 50 output_hevc_small.mp4
```

## Script de Geração em Lote

Para codificar múltiplos codecs de uma vez, use o script:
```bash
./dataset/generate.sh -i original.yuv -s 1920x1080
```

Opções:
- `-i`: Arquivo YUV de entrada (obrigatório)
- `-s`: Resolução de saída (padrão: 3840x2160)
- `-e`: Codificador específico (mjpeg, vvc, av1, vp9, avc, hevc, jpegxs)

### Exemplos do script

```bash
# Gerar todos os codecs em 4K
./dataset/generate.sh -i original.yuv -s 3840x2160

# Gerar apenas AV1 em 1080p
./dataset/generate.sh -i original.yuv -s 1920x1080 -e av1

# Gerar HEVC e VVC (executar duas vezes)
./dataset/generate.sh -i original.yuv -s 1920x1080 -e hevc
./dataset/generate.sh -i original.yuv -s 1920x1080 -e vvc
```