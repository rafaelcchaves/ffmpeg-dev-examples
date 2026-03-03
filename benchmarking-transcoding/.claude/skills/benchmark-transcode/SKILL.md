---
name: benchmark-transcode
description: Executar benchmarks de transcodificação de vídeo com diferentes encoders e perfis de threads. Use para medir desempenho de transcodificação e analisar resultados.
---

# Benchmark de Transcodificação

## Visão Geral

O script `transcode.sh` executa benchmarks de transcodificação de vídeo, testando diferentes encoders e perfis de threads. Ele compila e executa o código em `src/transcode.c` para cada configuração de perfil.

### Arquivos Relacionados

| Arquivo | Descrição |
|---------|-----------|
| `transcode.sh` | Script principal de orquestração |
| `src/transcode.c` | Código C de transcodificação (FFmpeg + LZ4) |
| `results/transcoding/analyze_transcoding.py` | Análise e visualização dos resultados |

---

## Uso Básico

```bash
./transcode.sh -i <input-video> -r <csv-result> -e <encoder> [-o <output-dir>]
```

### Parâmetros

| Parâmetro | Obrigatório | Descrição |
|-----------|-------------|-----------|
| `-i` | Sim | Arquivo de vídeo de entrada |
| `-r` | Sim | Arquivo CSV para salvar resultados |
| `-e` | Sim | Encoder a ser usado |
| `-o` | Não | Diretório de saída (default: `.`) |

---

## Encoders Suportados

| Encoder | Descrição | Extensão de Saída |
|---------|-----------|-------------------|
| `mjpeg` | Motion JPEG (FFmpeg nativo) | `.mjpeg` |
| `libsvtjpegxs` | JPEG XS (SVT) | `.jpegxs` |
| `lz4` | Compressão LZ4 rápida | `.lz4` |
| `lz4hc` | Compressão LZ4 alta compressão | `.lz4hc` |

### Detalhes dos Encoders

**MJPEG (`mjpeg`)**
- Codec padrão do FFmpeg
- Usa `FF_QP2LAMBDA * 2` como qualidade
- Formato de pixel: `YUVJ420P`

**JPEG XS (`libsvtjpegxs`)**
- Codec de baixa latência da Intel
- Configurado com 4 bpp (bits per pixel)
- Formato de pixel: `YUV420P`

**LZ4 (`lz4`)**
- Compressão sem perdas muito rápida
- Usa aceleração configurável via `LZ_CONFIG`
- Gera arquivo customizado com header por frame

**LZ4HC (`lz4hc`)**
- LZ4 com alta compressão (High Compression)
- Nível de compressão máximo: 12
- Trade-off: menor tamanho, maior tempo de compressão

---

## Perfis de Threads

O script testa automaticamente três perfis de threading:

| Perfil | Threads IN | Threads OUT | Uso Recomendado |
|--------|------------|-------------|-----------------|
| `low_latency` | 1 | 1 | Menor latência por frame |
| `balanced` | 8 | 8 | Equilíbrio latência/throughput |
| `high_throughput` | 16 | 16 | Máximo throughput |

### Notas sobre LZ4/LZ4HC

Para encoders LZ4, os perfis têm comportamento especial:
- **LZ4**: `threads_out` é sempre 1 (não implementado multi-thread)
- **LZ4HC**: Nível de compressão limitado a 12

---

## Formato do CSV de Saída

### Para MJPEG e JPEG XS

```csv
profile,threads_in,threads_out,type,time
low_latency,1,1,transcoding,1234
low_latency,1,1,total,567890
low_latency,1,1,fps,60.5
```

### Para LZ4

```csv
profile,threads_in,threads_out,type,time,acceleration
low_latency,1,1,transcoding,1234,1
```

### Para LZ4HC

```csv
profile,threads_in,threads_out,type,time,compression_level
low_latency,1,1,transcoding,1234,1
```

### Colunas

| Coluna | Descrição |
|--------|-----------|
| `profile` | Nome do perfil (`low_latency`, `balanced`, `high_throughput`) |
| `threads_in` | Número de threads de decodificação |
| `threads_out` | Número de threads de codificação |
| `type` | Tipo de métrica (`transcoding`, `total`, `fps`) |
| `time` | Tempo em microssegundos (para `transcoding`/`total`) ou FPS (para `fps`) |
| `acceleration` | Nível de aceleração LZ4 (apenas encoder `lz4`) |
| `compression_level` | Nível de compressão (apenas encoder `lz4hc`) |

---

## Análise de Resultados

### Usando analyze_transcoding.py

```bash
python3 results/transcoding/analyze_transcoding.py resultado1.csv resultado2.csv ...
```

### Saídas do Script de Análise

1. **Estatísticas Resumidas**
   - FPS médio por perfil

2. **Gráfico de FPS por Perfil**
   - Bar chart comparando throughput

3. **Gráfico de Tempo Médio de Frame**
   - Latência média por perfil (em ms)

4. **Box Plot de Distribuição**
   - Distribuição dos tempos de frame

### Dependências

```bash
pip install pandas matplotlib seaborn
```

---

## Exemplos Práticos

### Exemplo 1: Transcodificação MJPEG

```bash
# Transcodificar vídeo H.264 para MJPEG
./transcode.sh \
  -i dataset/videos/foreman.mp4 \
  -r results/transcoding/h264-mjpeg.csv \
  -e mjpeg \
  -o output/
```

### Exemplo 2: Transcodificação JPEG XS

```bash
# Transcodificar para JPEG XS
./transcode.sh \
  -i dataset/videos/foreman.mp4 \
  -r results/transcoding/h264-jpegxs.csv \
  -e libsvtjpegxs \
  -o output/
```

### Exemplo 3: Compressão LZ4

```bash
# Compressão rápida com LZ4
./transcode.sh \
  -i dataset/videos/foreman.mp4 \
  -r results/transcoding/h264-lz4.csv \
  -e lz4 \
  -o output/
```

### Exemplo 4: Análise de Múltiplos Resultados

```bash
# Analisar múltiplos CSVs de uma vez
python3 results/transcoding/analyze_transcoding.py \
  results/transcoding/h264-mjpeg.csv \
  results/transcoding/h264-jpegxs.csv \
  results/transcoding/h264-lz4.csv
```

---

## Arquitetura do Código (src/transcode.c)

### Fluxo de Processamento

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  Demuxer    │ ──► │  Decoder     │ ──► │  Encoder    │ ──► Output
│ (FFmpeg)    │     │  (FFmpeg)    │     │  (FFmpeg/   │
└─────────────┘     └──────────────┘     │   LZ4)      │
                                         └─────────────┘
```

### Funções Principais

| Função | Descrição |
|--------|-----------|
| `main()` | Inicialização, parsing de argumentos, loop principal |
| `transcode()` | Decodifica frame, codifica no formato de saída |

### Macros de Compilação

| Macro | Default | Descrição |
|-------|---------|-----------|
| `THREADS_IN` | 0 | Threads de decodificação |
| `THREADS_OUT` | 0 | Threads de codificação |
| `USE_LZ_COMPRESS` | - | Habilita compressão LZ4 |
| `LZ_CONFIG` | 1 | Nível de aceleração/compressão LZ4 |

---

## Verificação de Qualidade

Para verificar qualidade após transcodificação:

```bash
# PSNR
ffmpeg -i output/high_throughput.mjpeg -i dataset/videos/foreman.mp4 -lavfi psnr -f null -

# SSIM
ffmpeg -i output/high_throughput.mjpeg -i dataset/videos/foreman.mp4 -lavfi ssim -f null -
```
