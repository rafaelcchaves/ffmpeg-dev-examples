# Detalhes Avançados dos Codecs de Vídeo

Este documento fornece informações técnicas detalhadas sobre cada codec suportado.

---

## MJPEG (Motion JPEG)

### Características
- **Tipo**: Intra-frame (cada frame é independente)
- **Compressão**: Baseada em DCT (Discrete Cosine Transform)
- **Latência**: Muito baixa (ideal para edição)
- **Uso típico**: Edição de vídeo, captura, streaming de baixa latência

### Parâmetros de Qualidade

| Parâmetro | Range | Padrão | Descrição |
|-----------|-------|--------|-----------|
| `-q:v` | 1-31 | 2 | Qualidade (menor = melhor) |

### Trade-offs
- **Vantagens**: Edição frame-precise, baixa latência, ampla compatibilidade
- **Desvantagens**: Baixa eficiência de compressão, arquivos grandes

### Exemplos de Configuração

```bash
# Alta qualidade (arquivo maior)
-q:v 2

# Qualidade balanceada
-q:v 10

# Menor qualidade (arquivo menor)
-q:v 20
```

---

## AVC / H.264 (libx264)

### Características
- **Tipo**: Inter-frame com predição espacial e temporal
- **Compressão**: Baseada em DCT, motion estimation avançado
- **Latência**: Variável (depende do GOP)
- **Uso típico**: Streaming, broadcast, Blu-ray

### Parâmetros de Qualidade

| Parâmetro | Valores | Padrão | Descrição |
|-----------|---------|--------|-----------|
| `-preset` | ultrafast...veryslow | medium | Trade-off velocidade/compressão |
| `-crf` | 0-51 | 23 | Constant Rate Factor (menor = melhor) |
| `-tune` | film, animation, grain, stillimage | - | Otimizações para tipo de conteúdo |

### Presets e Impacto

| Preset | Velocidade | Compressão | Uso Recomendado |
|--------|------------|------------|-----------------|
| ultrafast | Muito rápida | -30% | Live streaming |
| fast | Rápida | -15% | Encoding rápido |
| medium | Média | Base | Uso geral |
| slow | Lenta | +5-10% | Arquivamento |
| veryslow | Muito lenta | +10-15% | Máxima qualidade |

### CRF e Qualidade

| CRF | Qualidade | Uso |
|-----|-----------|-----|
| 18-22 | Visualmente lossless | Arquivamento, pós-produção |
| 23 | Alta qualidade (padrão) | Uso geral |
| 24-28 | Boa qualidade | Web, streaming |
| 29+ | Qualidade reduzida | Bandwidth limitado |

### Exemplos de Configuração

```bash
# Alta qualidade para arquivamento
-preset slow -crf 18

# Streaming ao vivo
-preset veryfast -crf 23

# Animações
-preset medium -crf 20 -tune animation

# Filme com granulação
-preset slow -crf 20 -tune grain
```

---

## HEVC / H.265 (libx265)

### Características
- **Tipo**: Inter-frame com CTUs (Coding Tree Units)
- **Compressão**: ~50% mais eficiente que H.264
- **Latência**: Maior que H.264
- **Uso típico**: 4K/8K, streaming de alta qualidade

### Parâmetros de Qualidade

| Parâmetro | Valores | Padrão | Descrição |
|-----------|---------|--------|-----------|
| `-preset` | ultrafast...veryslow | medium | Trade-off velocidade/compressão |
| `-crf` | 0-51 | 28 | Constant Rate Factor (menor = melhor) |
| `-x265-params` | Ver documentação | - | Parâmetros avançados |

### CRF Equivalência H.264 → H.265

Como HEVC é ~50% mais eficiente:
- H.264 CRF 23 ≈ H.265 CRF 28
- Adicionar ~5 ao CRF do H.264 para equivalência

### Exemplos de Configuração

```bash
# 4K alta qualidade
-preset slow -crf 22

# Streaming 1080p
-preset medium -crf 26

# Máxima compressão
-preset veryslow -crf 28
```

---

## AV1 (libsvtav1)

### Características
- **Tipo**: Inter-frame com predição avançada
- **Compressão**: ~30% melhor que HEVC
- **Latência**: Alta (encoding lento)
- **Uso típico**: YouTube, Netflix, streaming de última geração

### Parâmetros de Qualidade

| Parâmetro | Valores | Padrão | Descrição |
|-----------|---------|--------|-----------|
| `-preset` | 0-13 | 6 | Velocidade (0 = melhor qualidade) |
| `-crf` | 0-63 | 35 | Constant Rate Factor |

### Presets SVT-AV1

| Preset | Velocidade | Qualidade | Uso |
|--------|------------|-----------|-----|
| 0-2 | Muito lento | Máxima | Arquivamento profissional |
| 3-5 | Lento | Alta | Alta qualidade |
| 6-8 | Médio | Boa | Uso geral |
| 9-11 | Rápido | Moderada | Encoding rápido |
| 12-13 | Muito rápido | Reduzida | Live streaming |

### Exemplos de Configuração

```bash
# Arquivamento profissional
-preset 2 -crf 25

# Uso geral
-preset 6 -crf 35

# Rápido para testes
-preset 10 -crf 40
```

---

## VP9 (libvpx-vp9)

### Características
- **Tipo**: Inter-frame (open source)
- **Compressão**: Similar a HEVC
- **Latência**: Média a alta
- **Uso típico**: YouTube, WebRTC

### Parâmetros de Qualidade

| Parâmetro | Valores | Padrão | Descrição |
|-----------|---------|--------|-----------|
| `-cpu-used` | 0-5 | 1 | Trade-off velocidade/qualidade |
| `-crf` | 0-63 | 31 | Requer `-b:v 0` |
| `-deadline` | good, best, realtime | good | Modo de encoding |

### CPU-Used

| Valor | Velocidade | Qualidade |
|-------|------------|-----------|
| 0 | Muito lento | Melhor |
| 1-2 | Lento | Alta |
| 3-4 | Médio | Boa |
| 5 | Rápido | Reduzida |

### Exemplos de Configuração

```bash
# Alta qualidade
-cpu-used 1 -b:v 0 -crf 25

# Uso geral
-cpu-used 4 -b:v 0 -crf 31

# Dois-passos para melhor qualidade (recomendado para VOD)
# Passo 1:
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -c:v libvpx-vp9 -pass 1 -b:v 0 -crf 30 -f null /dev/null
# Passo 2:
ffmpeg -f rawvideo -r 50 -pixel_format yuv420p -video_size WxH -i input.yuv \
  -c:v libvpx-vp9 -pass 2 -b:v 0 -crf 30 output_vp9.webm
```

---

## VVC (libvvenc)

### Características
- **Tipo**: Inter-frame de última geração
- **Compressão**: ~50% melhor que HEVC
- **Latência**: Alta
- **Uso típico**: Broadcasting futuro, arquivamento

### Parâmetros de Qualidade

| Parâmetro | Valores | Padrão | Descrição |
|-----------|---------|--------|-----------|
| `-preset` | faster, fast, medium, slow, slower | medium | Trade-off velocidade/compressão |
| `-qp` | 0-51 | - | Quantization parameter |

### Presets

| Preset | Velocidade | Qualidade |
|--------|------------|-----------|
| faster | Mais rápido | Menor |
| fast | Rápido | Moderada |
| medium | Médio | Boa |
| slow | Lento | Alta |
| slower | Mais lento | Melhor |

### Exemplos de Configuração

```bash
# Alta qualidade
-preset slow

# Uso geral
-preset medium

# Rápido
-preset faster
```

---

## JPEG XS (libsvtjpegxs)

### Características
- **Tipo**: Intra-frame (lightweight)
- **Compressão**: Baixa latência, qualidade visual
- **Latência**: Muito baixa (< 1ms)
- **Uso típico**: Broadcast profissional, SDI over IP, live production

### Parâmetros de Qualidade

| Parâmetro | Valores | Padrão | Descrição |
|-----------|---------|--------|-----------|
| `-bpp` | 1-12 | 4 | Bits per pixel |

### BPP e Qualidade

| BPP | Compressão | Qualidade | Uso |
|-----|------------|-----------|-----|
| 1-2 | 12-24:1 | Reduzida | Testes |
| 3-4 | 6-8:1 | Boa | Broadcast |
| 5-6 | 4-5:1 | Alta | Produção |
| 8+ | 3:1 | Visualmente lossless | Arquivamento |

### Exemplos de Configuração

```bash
# Broadcast padrão
-bpp 4

# Alta qualidade
-bpp 6

# Visualmente lossless
-bpp 8
```

---

## Comparativo Geral

| Codec | Compressão | Velocidade | Latência | Uso Principal |
|-------|------------|------------|----------|---------------|
| MJPEG | Baixa | Muito rápida | Muito baixa | Edição |
| AVC | Boa | Rápida | Baixa | Streaming geral |
| HEVC | Muito boa | Média | Média | 4K, streaming |
| AV1 | Excelente | Lenta | Alta | Streaming novo |
| VP9 | Muito boa | Média | Média | Web, YouTube |
| VVC | Excelente | Lenta | Alta | Futuro broadcast |
| JPEG XS | Baixa | Muito rápida | Muito baixa | Live production |