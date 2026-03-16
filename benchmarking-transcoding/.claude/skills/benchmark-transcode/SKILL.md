---
name: benchmark-transcode
description: Executar benchmarks de transcodificação de vídeo com diferentes encoders e perfis de threads. Suporta LZ4/LZ4HC multithread. Use para medir desempenho de transcodificação e analisar resultados.
---

# Benchmark de Transcodificação

## Visão Geral

O script `transcode.sh` executa benchmarks de transcodificação de vídeo, testando diferentes encoders e perfis de threads. Para LZ4/LZ4HC, utiliza automaticamente a versão multithread quando `threads_out > 1`.

### Arquivos Relacionados

| Arquivo | Descrição |
|---------|-----------|
| `transcode.sh` | Script principal de orquestração |
| `src/transcode.c` | Código C de transcodificação single-thread (FFmpeg + LZ4) |
| `src/transcode_lz4/transcode_lz4_mt.c` | Código C de transcodificação multithread (LZ4/LZ4HC) |
| `src/transcode_lz4/transcode_lz4_main.c` | Ponto de entrada para versão multithread |
| `src/avbuffer_queue.c` | Fila thread-safe para buffers AVBufferRef |
| `results/transcoding/analyze_transcoding.py` | Análise e visualização dos resultados |

---

## Uso Básico

```bash
./transcode.sh -i <input-video> -r <csv-result> -e <encoder> [-o <output-dir>] [-w]
```

### Parâmetros

| Parâmetro | Obrigatório | Descrição |
|-----------|-------------|-----------|
| `-i` | Sim | Arquivo de vídeo de entrada |
| `-r` | Sim | Arquivo CSV para salvar resultados |
| `-e` | Sim | Encoder a ser usado |
| `-o` | Não | Diretório de saída (default: `.`) |
| `-w` | Não | Habilita escrita de arquivos de saída (default: desabilitado) |

### Flag de Escrita (`-w`)

Por padrão, os benchmarks são executados **sem escrita de arquivos** para medir apenas o desempenho de CPU:

```bash
# Benchmark puro (padrão) - sem I/O de disco
./transcode.sh -i video.mp4 -r results.csv -e lz4 -o output/
# Arquivos de saída terão tamanho 0 ou não existirão
```

Para habilitar a escrita de arquivos:

```bash
# Com escrita habilitada
./transcode.sh -i video.mp4 -r results.csv -e lz4 -o output/ -w
# Arquivos de saída serão gerados normalmente
```

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
- Suporta multithread quando `threads_out > 1`
- Usa aceleração configurável via `LZ_CONFIG`
- Gera arquivo customizado com header por frame

**LZ4HC (`lz4hc`)**
- LZ4 com alta compressão (High Compression)
- Suporta multithread quando `threads_out > 1`
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

### Seleção de Binário para LZ4/LZ4HC

O script escolhe automaticamente entre single-thread e multithread:

| threads_out | Binário | Descrição |
|-------------|---------|-----------|
| 1 | `transcode.c` | Single-thread clássico |
| > 1 | `transcode_lz4_mt` | Multithread producer-consumer |

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

### Exemplo 3: Compressão LZ4 (Multithread)

```bash
# Compressão com LZ4 - balanced e high_throughput usam multithread
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

## Arquitetura do Código

### Single-Thread (transcode.c)

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  Demuxer    │ ──► │  Decoder     │ ──► │  Encoder    │ ──► Output
│ (FFmpeg)    │     │  (FFmpeg)    │     │  (FFmpeg/   │
└─────────────┘     └──────────────┘     │   LZ4)      │
                                         └─────────────┘
```

### Multithread (transcode_lz4_mt)

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  Producer   │ ──► │  FrameQueue  │ ──► │  Encoders   │ ──► Output
│  Thread     │     │  (thread-    │     │  (múltiplas │     (sequencial)
│             │     │   safe)      │     │   threads)  │
└─────────────┘     └──────────────┘     └─────────────┘
      │                                          │
      ▼                                          ▼
  Decodifica                               Comprime LZ4
  (FFmpeg)                                 e escreve
```

### Componentes Multithread

| Componente | Arquivo | Descrição |
|------------|---------|-----------|
| Producer Thread | `transcode_lz4_mt.c` | Decodifica frames com FFmpeg |
| FrameQueue | `avbuffer_queue.c` | Fila thread-safe com buffer + metadata |
| Encoder Threads | `transcode_lz4_mt.c` | Compressão LZ4/LZ4HC paralela |
| Write Mutex | `transcode_lz4_mt.c` | Garante escrita sequencial |

### Funções Principais (transcode.c)

| Função | Descrição |
|--------|-----------|
| `main()` | Inicialização, parsing de argumentos, loop principal |
| `transcode()` | Decodifica frame, codifica no formato de saída |

### Funções Principais (transcode_lz4_mt)

| Função | Descrição |
|--------|-----------|
| `mt_encode_main()` | Orquestra threads e sincronização |
| `mt_producer_thread()` | Decodifica frames e coloca na fila |
| `mt_encoder_thread()` | Retira frames, comprime e escreve |

### Macros de Compilação

#### transcode.c

| Macro | Default | Descrição |
|-------|---------|-----------|
| `THREADS_IN` | 0 | Threads de decodificação |
| `THREADS_OUT` | 0 | Threads de codificação |
| `USE_LZ_COMPRESS` | - | Habilita compressão LZ4 |
| `LZ_CONFIG` | 1 | Nível de aceleração/compressão LZ4 |
| `ENABLE_OUTPUT_WRITE` | 0 | Habilita escrita de arquivos de saída |

#### transcode_lz4_mt

| Macro | Default | Descrição |
|-------|---------|-----------|
| `THREADS_IN` | 0 | Threads de decodificação FFmpeg |
| `THREADS_OUT` | 1 | Threads codificadoras LZ4 |
| `ENABLE_OUTPUT_WRITE` | 0 | Habilita escrita de arquivos de saída |

---

## Compilação Manual

### Single-Thread (transcode.c)

```bash
# LZ4
gcc -O3 -DTHREADS_IN=4 -DTHREADS_OUT=1 -DUSE_LZ_COMPRESS -DLZ_CONFIG=1 \
    src/transcode.c -o transcode -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -llzo2

# LZ4HC
gcc -O3 -DTHREADS_IN=4 -DTHREADS_OUT=1 -DUSE_LZ_COMPRESS -DLZ_CONFIG=9 \
    src/transcode.c -o transcode -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -llzo2
```

### Multithread (transcode_lz4_mt)

```bash
# LZ4 com 8 threads codificadoras
gcc -O3 -DTHREADS_IN=4 -DTHREADS_OUT=8 \
    src/transcode_lz4/transcode_lz4_main.c \
    src/transcode_lz4/transcode_lz4_mt.c \
    src/avbuffer_queue.c \
    src/cpu_stats.cpp \
    -o transcode_lz4_mt \
    -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -lpthread
```

---

## Verificação de Qualidade

Para verificar qualidade após transcodificação:

```bash
# PSNR
ffmpeg -i output/high_throughput.mjpeg -i dataset/videos/foreman.mp4 -lavfi psnr -f null -

# SSIM
ffmpeg -i output/high_throughput.mjpeg -i dataset/videos/foreman.mp4 -lavfi ssim -f null -
```
