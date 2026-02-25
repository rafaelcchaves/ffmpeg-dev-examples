---
name: video-compare
description: Comparação de qualidade de vídeos usando SSIM com o script compare.sh. Use quando precisar comparar vídeos codificados contra uma referência original e medir perdas de qualidade.
---

# Comparação de Qualidade de Vídeo com SSIM

## O Script compare.sh

Localização: `dataset/compare.sh`

### Uso
```bash
./compare.sh <WxH> <reference.yuv> <codec1> <file1> [<codec2> <file2> ...]
```

### Parâmetros
- **WxH**: Resolução do vídeo (ex: 1920x1080)
- **reference.yuv**: Arquivo YUV de referência (original descomprimido)
- **codec1, file1**: Nome do codec e arquivo comprimido para comparar
- **codec2, file2, ...**: Pares adicionais de codec/arquivo (opcional)

### Como Funciona
1. Decodifica cada arquivo comprimido para YUV bruto
2. Compara o YUV decodificado com a referência usando SSIM
3. Exibe tamanhos dos arquivos e scores SSIM

## Exemplos

### Comparar um único codec
```bash
./compare.sh 1920x1080 original.yuv av1 output_av1.mp4
```

### Comparar múltiplos codecs
```bash
./compare.sh 3840x2160 original.yuv \
  av1 3840x2160_av1.mp4 \
  hevc 3840x2160_hevc.mp4 \
  vvc 3840x2160_vvc.mp4
```

### Comparar todos os codecs de uma vez
```bash
./compare.sh 1920x1080 original.yuv \
  mjpeg 1920x1080_mjpeg.mp4 \
  avc 1920x1080_avc.mp4 \
  hevc 1920x1080_hevc.mp4 \
  av1 1920x1080_av1.mp4 \
  vp9 1920x1080_vp9.mp4 \
  vvc 1920x1080_vvc.mp4 \
  jpegxs 1920x1080_jpegxs.mp4
```

### Comparação em 4K
```bash
./compare.sh 3840x2160 original.yuv \
  av1 3840x2160_av1.mp4 \
  hevc 3840x2160_hevc.mp4 \
  vvc 3840x2160_vvc.mp4
```

## Interpretação do SSIM

### Tabela de Qualidade

| SSIM | Qualidade | Descrição |
|------|-----------|-----------|
| > 0.99 | Excelente | Praticamente indistinguível do original |
| 0.98 - 0.99 | Muito alta | Diferenças imperceptíveis |
| 0.95 - 0.98 | Alta | Leves diferenças em detalhes finos |
| 0.90 - 0.95 | Boa | Leves artefatos visíveis sob inspeção |
| 0.85 - 0.90 | Aceitável | Artefatos visíveis, qualidade degradada |
| 0.80 - 0.85 | Moderada | Artefatos evidentes |
| < 0.80 | Baixa | Qualidade significativamente degradada |

### Valores Típicos por Codec

| Codec | SSIM Típico (4K) | Compressão |
|-------|------------------|------------|
| MJPEG | 0.95-0.98 | ~10:1 |
| AVC | 0.96-0.99 | ~50:1 |
| HEVC | 0.95-0.98 | ~100:1 |
| AV1 | 0.94-0.98 | ~150:1 |
| VP9 | 0.95-0.98 | ~100:1 |
| VVC | 0.94-0.97 | ~200:1 |
| JPEG XS | 0.97-0.99 | ~8:1 |

## Entendendo o Output

### Exemplo de Saída
```
File Sizes:
   raw: 7.0G
Decoding comparison file: 3840x2160_av1.mp4 (codec: av1)
  av1 (compressed): 16M
SSIM (av1 vs original.yuv):
All:0.98 (04:02:03:04)
```

### Interpretação
- **raw**: Tamanho do arquivo original descomprimido
- **compressed**: Tamanho após codificação
- **All:0.98**: Score SSIM médio (0.98 = 98% de similaridade)
- **(04:02:03:04)**: SSIM por componente (Y:U:V ou similar)

## Workflow Típico

### 1. Gere os vídeos codificados
Use a skill `video-generate` ou o script `generate.sh`:

```bash
# Gerar todos os codecs
./dataset/generate.sh -i original.yuv -s 3840x2160

# Ou gerar codecs específicos
./dataset/generate.sh -i original.yuv -s 1920x1080 -e av1
./dataset/generate.sh -i original.yuv -s 1920x1080 -e hevc
```

### 2. Execute compare.sh contra o YUV original
```bash
cd dataset
./compare.sh 3840x2160 original.yuv \
  av1 3840x2160_av1.mp4 \
  hevc 3840x2160_hevc.mp4 \
  vvc 3840x2160_vvc.mp4
```

### 3. Analise trade-offs entre tamanho e qualidade

Compare:
- **Taxa de compressão**: Tamanho original / Tamanho comprimido
- **Qualidade**: Score SSIM
- **Eficiência**: SSIM / Tamanho (maior = melhor)

## Comparação Manual com FFmpeg

Se precisar de mais controle, pode executar o SSIM diretamente:

```bash
# Decodificar vídeo comprimido para YUV
ffmpeg -i compressed.mp4 decoded.yuv

# Calcular SSIM
ffmpeg -f rawvideo -s 1920x1080 -i decoded.yuv \
       -f rawvideo -s 1920x1080 -i original.yuv \
       -lavfi ssim -f null -
```

### Opções Avançadas do SSIM

```bash
# SSIM com arquivo de log
ffmpeg -f rawvideo -s 1920x1080 -i decoded.yuv \
       -f rawvideo -s 1920x1080 -i original.yuv \
       -lavfi ssim=stats_file=ssim_log.txt -f null -

# SSIM com pesos personalizados
ffmpeg -f rawvideo -s 1920x1080 -i decoded.yuv \
       -f rawvideo -s 1920x1080 -i original.yuv \
       -lavfi "ssim=weight_y=0.8:weight_u=0.1:weight_v=0.1" -f null -
```

## Outras Métricas de Qualidade

### PSNR (Peak Signal-to-Noise Ratio)
```bash
ffmpeg -f rawvideo -s 1920x1080 -i decoded.yuv \
       -f rawvideo -s 1920x1080 -i original.yuv \
       -lavfi psnr -f null -
```

| PSNR (dB) | Qualidade |
|-----------|-----------|
| > 50 | Excelente |
| 40-50 | Muito boa |
| 30-40 | Boa |
| 20-30 | Aceitável |
| < 20 | Ruim |

### VMAF (Video Multimethod Assessment Fusion)
Requer Netflix vmaf-filter:

```bash
ffmpeg -i compressed.mp4 -i reference.mp4 \
       -lavfi libvmaf -f null -
```

## Dicas e Boas Práticas

### 1. Use sempre a mesma resolução
O SSIM requer que ambos os vídeos tenham as mesmas dimensões.

### 2. Compare na resolução de visualização
Se o vídeo será exibido em 1080p, compare em 1080p mesmo que o original seja 4K.

### 3. Considere múltiplas métricas
- SSIM para similaridade estrutural
- PSNR para erro de pixel
- VMAF para percepção humana (quando disponível)

### 4. Avalie visualmente
Métricas não capturam todos os aspectos visuais. Sempre verifique amostras visualmente.

### 5. Documente os parâmetros
Ao comparar, registre os presets e CRF usados para reprodutibilidade.