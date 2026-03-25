---
name: benchmark-transcode
description: Executar benchmarks de transcodificação de vídeo com diferentes encoders e perfis de threads. Suporta LZ4/LZ4HC multithread. Use para medir desempenho de transcodificação e analisar resultados.
---

# Benchmark de Transcodificação

## Visão Geral

O script `transcode.sh` executa benchmarks de transcodificação de vídeo, testando diferentes encoders e perfis de threads. Para LZ4/LZ4HC, utiliza a versão multithread com threads configuradas via CLI (`-D` decoder, `-E` encoder).

### Arquivos Relacionados

| Arquivo | Descrição |
|---------|-----------|
| `transcode.sh` | Script principal de orquestração |
| `src/transcode.c` | Código C de transcodificação (FFmpeg + encoders não-LZ4) |
| `src/transcode_lz4/transcode_lz4_mt.c` | Código C de transcodificação multithread (LZ4/LZ4HC) |
| `src/transcode_lz4/transcode_lz4_main.c` | Ponto de entrada para versão multithread |
| `src/queue.c` | Fila genérica thread-safe |
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
- Suporta multithread via produtor-consumidor
- Usa aceleração configurável via `-l`
- Gera arquivo customizado com header por frame

**LZ4HC (`lz4hc`)**
- LZ4 com alta compressão (High Compression)
- Suporta multithread via produtor-consumidor
- Nível de compressão máximo: 12
- Trade-off: menor tamanho, maior tempo de compressão

---

## Perfis de Threads

O script testa automaticamente três perfis de threading:

| Perfil | Decoder Threads | Encoder Threads | Uso Recomendado |
|--------|-----------------|-----------------|-----------------|
| `low_latency` | 1 | 1 | Menor latência por frame |
| `balanced` | 8 | 8 | Equilíbrio latência/throughput |
| `high_throughput` | 16 | 16 | Máximo throughput |

### Seleção de Binário para LZ4/LZ4HC

O script sempre usa o binário multithread (`transcode_lz4_mt`) para LZ4/LZ4HC, passando as contagens de threads via CLI.

---

## Formato do CSV de Saída

### Para MJPEG e JPEG XS

```csv
profile,decoder_threads,encoder_threads,type,time
low_latency,1,1,transcoding,1234
low_latency,1,1,total,567890
low_latency,1,1,fps,60.5
```

### Para LZ4

```csv
profile,decoder_threads,encoder_threads,type,time,acceleration
low_latency,1,1,transcoding,1234,1
```

### Para LZ4HC

```csv
profile,decoder_threads,encoder_threads,type,time,compression_level
low_latency,1,1,transcoding,1234,1
```

### Colunas

| Coluna | Descrição |
|--------|-----------|
| `profile` | Nome do perfil (`low_latency`, `balanced`, `high_throughput`) |
| `decoder_threads` | Número de threads de decodificação |
| `encoder_threads` | Número de threads de codificação |
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
   - Contagem de threads por perfil

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
./transcode.sh \
  -i dataset/ssim95_avc.mp4 \
  -r results/transcoding/avc-mjpeg.csv \
  -e mjpeg \
  -o output/
```

### Exemplo 2: Compressão LZ4 (Multithread)

```bash
./transcode.sh \
  -i dataset/ssim95_avc.mp4 \
  -r results/transcoding/avc-lz4.csv \
  -e lz4 \
  -o output/
```

### Exemplo 3: Análise de Múltiplos Resultados

```bash
python3 results/transcoding/analyze_transcoding.py \
  results/transcoding/avc-mjpeg.csv \
  results/transcoding/avc-lz4.csv
```

---

## Arquitetura do Código

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

### Funções Principais (transcode_lz4_mt)

| Função | Descrição |
|--------|-----------|
| `mt_encode_main()` | Orquestra threads e sincronização |
| `mt_producer_thread()` | Decodifica frames e coloca na fila |
| `mt_encoder_thread()` | Retira frames, comprime e escreve |

### Opções de CLI para Threads

| Opção | Default | Descrição |
|-------|---------|-----------|
| `-D <N>` | 0 (auto) | Threads decodificadoras FFmpeg |
| `-E <N>` | 1 | Threads codificadoras LZ4 |

---

## Compilação Manual

### LZ4/LZ4HC Multithread

```bash
gcc -O3 -Wall -Wno-unused-variable \
    src/transcode_lz4/transcode_lz4_main.c \
    src/transcode_lz4/transcode_lz4_mt.c \
    src/queue.c src/cpu_stats.cpp \
    -o transcode_mt \
    -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -lpthread
```

### Outros Encoders (transcode.c)

```bash
gcc -O3 -Wall -Wno-unused-variable src/transcode.c -o transcode \
    -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -llzo2
```

---

## Verificação de Qualidade

Para verificar qualidade após transcodificação:

```bash
# PSNR
ffmpeg -i output/high_throughput.mjpeg -i dataset/ssim95_avc.mp4 -lavfi psnr -f null -

# SSIM
ffmpeg -i output/high_throughput.mjpeg -i dataset/ssim95_avc.mp4 -lavfi ssim -f null -
```
