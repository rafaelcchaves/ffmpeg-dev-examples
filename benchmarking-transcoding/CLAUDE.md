# Manual de Referência - Benchmarking de Transcodificação de Vídeo

## Sumário do Repositório

Este projeto implementa um sistema de benchmarking para transcodificação e decodificação de vídeo utilizando FFmpeg e compressão LZ4. O foco é medir desempenho de CPU com arquiteturas single-thread e multithread.

### Características Principais

- **Transcodificação**: Suporte a MJPEG, JPEG XS, LZ4 e LZ4HC
- **Decodificação**: FFmpeg padrão e LZ4 multithread
- **Arquitetura Multithread**: Producer-consumer com fila genérica (Queue)
- **Flag de Escrita**: Permite benchmarks sem I/O de disco (padrão)
- **Perfis de Threads**: low_latency, balanced, high_throughput
- **Threads via CLI**: Configuração de threads em tempo de execução (`-D` decoder, `-E` encoder)

### Estrutura do Projeto

```
benchmarking-transcoding/
├── src/
│   ├── frame_types.h            # Tipos compartilhados (FrameHeader, FrameItem)
│   ├── queue.h / queue.c        # Fila genérica thread-safe com ownership
│   ├── transcode.c              # Transcodificação single-thread
│   ├── decode.cpp               # Decodificação FFmpeg
│   ├── transcode_lz4/           # Transcodificação LZ4 (single-thread + multithread)
│   │   ├── transcode_lz4_main.c
│   │   ├── transcode_lz4_st.c   # Encode single-thread
│   │   ├── transcode_lz4_st.h
│   │   └── transcode_lz4_mt.c   # Encode multithread (producer-consumer)
│   ├── decode_lz4/              # Decodificação LZ4 multithread
│   │   ├── decode_lz4_main.cpp
│   │   ├── decode_lz4_mt.cpp
│   │   ├── frame_reader.cpp
│   │   ├── frame_decoder.cpp
│   │   ├── frame_writer.cpp
│   │   └── stats.cpp
│   └── cpu_stats.cpp            # Coleta de uso de CPU
├── dataset/                     # Vídeos de teste
├── results/                     # Scripts de análise
│   ├── transcoding/
│   └── decoding/
├── transcode.sh                 # Script de benchmark de transcodificação
└── decode.sh                    # Script de benchmark de decodificação
```

### Arquitetura Multithread

O sistema multithread utiliza uma arquitetura **producer-consumer** com uma fila genérica thread-safe (`Queue`) que transporta `FrameItem` entre threads.

```
Encode path:
  Producer (FFmpeg decode)  →  Queue<FrameItem>  →  Consumers (LZ4/LZ4HC compress)

Decode path:
  Producer (file reader)    →  Queue<FrameItem>  →  Consumers (LZ4 decompress)
```

#### Tipos Compartilhados (`src/frame_types.h`)

| Tipo | Descrição |
|------|-----------|
| `FrameHeader` | Header on-disk de 20 bytes (size_decompress, size_compress, width, height, pix_fmt) |
| `FrameItem` | Item unificado para a fila (header + sequence_number + timestamp + AVBufferRef* data) |
| `frame_item_free()` | Libera um FrameItem e seu AVBufferRef (callback para Queue) |

- **Encode**: `FrameItem.data` aponta para pixels YUV brutos; o encoder usa `header.size_decompress`
- **Decode**: `FrameItem.data` aponta para dados LZ4 comprimidos; o decoder usa `header.size_compress`

#### Fila Genérica (`src/queue.h` / `src/queue.c`)

| Função | Descrição |
|--------|-----------|
| `queue_init(q, free_func)` | Inicializa fila com callback de liberação |
| `queue_destroy(q)` | Destrói fila, liberando itens restantes |
| `queue_push(q, item)` | Insere item (ownership transferido) |
| `queue_pop(q)` | Remove item (ownership retornado), bloqueante |
| `queue_clear(q)` | Limpa todos os itens |
| `queue_count(q)` | Número de elementos |
| `queue_is_empty(q)` | Verifica se vazia |

- **Ownership**: `queue_push` transfere ownership; `queue_pop` retorna ownership ao chamador
- **Poison pill**: `NULL` é aceito como sinal de término (sem chamar `free_func`)
- **Thread-safety**: Mutex + condition variable internos

#### Escrita Sequencial

Tanto o encode quanto o decode utilizam um mecanismo de **escrita sequencial** (`g_next_to_write` + `g_write_mutex` + `g_write_cond`) para garantir que os frames sejam escritos no arquivo em ordem, mesmo quando processados em paralelo por threads consumidoras.

---

## Dataset de Vídeos

### Localização

Os vídeos de teste estão em: `dataset/`

### Arquivos Disponíveis

| Arquivo | Descrição | Tamanho |
|---------|-----------|---------|
| `original.yuv` | Vídeo YUV bruto 4K (3840x2160) | ~7.5 GB |
| `3840x2160.lz4` | Vídeo comprimido com LZ4 | ~4.2 GB |
| `ssim95_avc.mp4` | H.264/AVC com SSIM >= 95 | ~16 MB |
| `ssim95_hevc.mp4` | H.265/HEVC com SSIM >= 95 | ~7 MB |
| `ssim95_mjpeg.mp4` | MJPEG com SSIM >= 95 | ~185 MB |
| `ssim95_jpegxs.mp4` | JPEG XS com SSIM >= 95 | ~311 MB |
| `ssim95_vp9.mp4` | VP9 com SSIM >= 95 | ~9 MB |

### Gerando Novos Vídeos

```bash
# Gerar vídeos a partir de YUV
./dataset/generate.sh -i dataset/original.yuv -s 1920x1080

# Comparar qualidade SSIM
./dataset/compare.sh 3840x2160 dataset/original.yuv \
    h264 dataset/ssim95_avc.mp4 \
    hevc dataset/ssim95_hevc.mp4
```

---

## Manual de Compilação

### Pré-requisitos

```bash
# Ubuntu/Debian
sudo apt install build-essential liblz4-dev

# FFmpeg compilado (ver build-ffmpeg.md)
# - libavcodec, libavutil, libavformat
# - codecs: libsvtjpegxs, mjpeg
```

### Compilação via Scripts (Recomendado)

Os scripts `transcode.sh` e `decode.sh` compilam automaticamente:

```bash
# Benchmark de transcodificação
./transcode.sh -i video.mp4 -r results.csv -e lz4 -o output/

# Benchmark de decodificação
./decode.sh -i video.mp4 -r results.csv -o output/
```

### Compilação Manual

#### Transcodificação - LZ4/LZ4HC (single-thread + multithread)

```bash
# Compilação única (escrita controlada via -w em tempo de execução)
gcc -O3 -Wall -Wno-unused-variable \
    src/transcode_lz4/transcode_lz4_main.c \
    src/transcode_lz4/transcode_lz4_st.c \
    src/transcode_lz4/transcode_lz4_mt.c \
    src/queue.c \
    src/cpu_stats.cpp \
    -o transcode_mt \
    -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -lpthread
```

#### Transcodificação - Outros Encoders (transcode.c)

```bash
# MJPEG
gcc -O3 -Wall -Wno-unused-variable src/transcode.c -o transcode \
    -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm
```

#### Decodificação - LZ4 Multithread

```bash
# Compilação única (escrita controlada via -w em tempo de execução)
g++ -O3 -Wall -Wno-unused-variable -Wno-unused-function \
    src/decode_lz4/decode_lz4_main.cpp src/decode_lz4/decode_lz4_mt.cpp \
    src/decode_lz4/frame_reader.cpp src/decode_lz4/frame_decoder.cpp \
    src/decode_lz4/frame_writer.cpp src/decode_lz4/stats.cpp \
    src/queue.c src/cpu_stats.cpp \
    -o decode_lz4 -I/usr/local/include -L/usr/local/lib \
    -lavutil -lm -llz4 -lpthread
```

#### Decodificação - FFmpeg (decode.cpp)

```bash
# Compilação única (escrita controlada via -w em tempo de execução)
g++ -O3 -Wall -Wno-unused-variable -Wno-unused-function \
    src/decode.cpp src/cpu_stats.cpp -o decode \
    -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm
```

### Opções de CLI para Threads

| Opção | Default | Descrição |
|-------|---------|-----------|
| `-D <N>` | 0 (auto) | Threads decodificadoras FFmpeg |
| `-E <N>` | 1 | Threads codificadoras LZ4 |
| `-w` | desabilitado | Habilita escrita de arquivos de saída |

---

## Manual de Execução

### Transcodificação

```bash
./transcode.sh -i <video> -r <csv> -e <encoder> [-o <dir>] [-w]
```

| Parâmetro | Descrição |
|-----------|-----------|
| `-i` | Vídeo de entrada |
| `-r` | Arquivo CSV de resultados |
| `-e` | Encoder: `mjpeg`, `libsvtjpegxs`, `lz4`, `lz4hc` |
| `-o` | Diretório de saída (default: `.`) |
| `-w` | Habilita escrita de arquivos (default: desabilitado) |

**Exemplos:**

```bash
# Benchmark LZ4 sem escrita (padrão)
./transcode.sh -i dataset/ssim95_avc.mp4 -r results/lz4.csv -e lz4 -o output/

# Transcodificação MJPEG com escrita
./transcode.sh -i dataset/ssim95_avc.mp4 -r results/mjpeg.csv -e mjpeg -o output/ -w

# LZ4HC com alta compressão
./transcode.sh -i dataset/ssim95_avc.mp4 -r results/lz4hc.csv -e lz4hc -o output/ -w
```

### Decodificação

```bash
./decode.sh -i <video> -r <csv> [-o <dir>] [-l <lz>] [-p <profile>] [-w]
```

| Parâmetro | Descrição |
|-----------|-----------|
| `-i` | Vídeo de entrada |
| `-r` | Arquivo CSV de resultados |
| `-o` | Diretório de saída (default: `.`) |
| `-l` | Algoritmo LZ: `lz4` ou `lz4hc` |
| `-p` | Perfil específico: `low_latency`, `balanced`, `high_throughput` |
| `-w` | Habilita escrita de arquivos (default: desabilitado) |

**Exemplos:**

```bash
# Decodificação FFmpeg padrão
./decode.sh -i dataset/ssim95_avc.mp4 -r results/decode.csv -o output/

# Decodificação LZ4
./decode.sh -i output/high_throughput.lz4 -r results/decode_lz4.csv -l lz4 -o output/

# Com escrita habilitada
./decode.sh -i output/high_throughput.lz4 -r results/decode_lz4.csv -l lz4 -o output/ -w
```

### Perfis de Threads

| Perfil | Decoder Threads | Encoder Threads | Uso |
|--------|-----------------|-----------------|-----|
| `low_latency` | 1 | 1 | Menor latência |
| `balanced` | 8 | 8 | Equilíbrio |
| `high_throughput` | 16 | 16 | Máximo throughput |

---

## Execução dos Binários Diretamente

### transcode_mt (LZ4/LZ4HC - single-thread + multithread)

```bash
./transcode_mt -i <input> [-o <output>] -e <encoder> -l <level> -p <profile> -D <decoder_threads> -E <encoder_threads> [-w]
```

| Parâmetro | Descrição |
|-----------|-----------|
| `-i` | Arquivo de entrada |
| `-o` | Arquivo de saída (default: output.lz4) |
| `-e` | Encoder: `lz4` ou `lz4hc` |
| `-l` | Nível de compressão (1-12) |
| `-p` | Nome do perfil |
| `-D` | Threads decodificadoras FFmpeg (default: 0 = auto) |
| `-E` | Threads codificadoras LZ4 (1 = single-thread, >1 = multithread) |
| `-w` | Habilita escrita de arquivo de saída |

### decode_lz4

```bash
./decode_lz4 -i <input> [-o <output>] -p <profile> -D <threads> [-w]
```

| Parâmetro | Descrição |
|-----------|-----------|
| `-i` | Arquivo .lz4 de entrada |
| `-o` | Arquivo .yuv de saída (default: output.yuv) |
| `-p` | Nome do perfil |
| `-D` | Número de threads decodificadoras (default: 1) |
| `-w` | Habilita escrita de arquivo de saída |

---

## Análise de Resultados

### Scripts de Análise

```bash
# Transcodificação
python3 results/transcoding/analyze_transcoding.py result1.csv result2.csv ...

# Decodificação
python3 results/decoding/analyze_decoding.py result1.csv result2.csv ...
```

### Dependências

```bash
pip install pandas matplotlib seaborn
```

### Formato do CSV

```csv
profile,decoder_threads,encoder_threads,type,time[,compression_level]
low_latency,1,1,transcoding,1234,1
low_latency,1,1,total,567890,1
low_latency,1,1,fps,60.5,1
```

---

## Workflow Completo

### Exemplo: Round-trip LZ4

```bash
# 1. Transcodificar com escrita
./transcode.sh \
    -i dataset/ssim95_avc.mp4 \
    -r results/encode.csv \
    -e lz4 \
    -o encoded/ \
    -w

# 2. Decodificar com escrita
./decode.sh \
    -i encoded/high_throughput.lz4 \
    -r results/decode.csv \
    -l lz4 \
    -o decoded/ \
    -w

# 3. Verificar SSIM
ffmpeg -s 1920x1080 -i decoded/high_throughput.yuv \
       -i dataset/ssim95_avc.mp4 \
       -lavfi ssim -f null -
```

### Exemplo: Benchmark Comparativo

```bash
# Transcodificar com múltiplos encoders
for enc in lz4 lz4hc mjpeg; do
    ./transcode.sh \
        -i dataset/ssim95_avc.mp4 \
        -r results/${enc}.csv \
        -e ${enc} \
        -o output/${enc}/
done

# Analisar todos juntos
python3 results/transcoding/analyze_transcoding.py \
    results/lz4.csv results/lz4hc.csv results/mjpeg.csv
```

---

## Skills Disponíveis

Use `/benchmark-transcode` e `/benchmark-decode` para executar benchmarks com assistência do Claude.

---

## Troubleshooting

### Erro: "Could not open codec"

Verifique se o FFmpeg foi compilado com os codecs necessários:

```bash
ffmpeg -encoders | grep -E "mjpeg|jpegxs"
```

### Erro: "undefined reference to `LZ4_compress_fast'"

Instale a biblioteca LZ4:

```bash
sudo apt install liblz4-dev
```

### Arquivos de saída com tamanho 0

Comportamento esperado quando `-w` não é especificado (benchmark puro).

### Baixo desempenho em multithread

Verifique se o número de threads não excede os núcleos disponíveis:

```bash
nproc  # Mostra número de CPUs lógicas
```
