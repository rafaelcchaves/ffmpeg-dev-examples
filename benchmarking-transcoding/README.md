# Benchmarking de Transcodificação de Vídeo

Este projeto contém um conjunto de scripts para realizar benchmarking de transcodificação e decodificação de vídeo utilizando FFmpeg e compilando programas em C.

## Estrutura do Projeto

-   `dataset/generate.sh`: Gera arquivos de vídeo codificados a partir de um arquivo YUV bruto.
-   `src/transcode.c` / `transcode.sh`: Realiza a transcodificação de um vídeo para diferentes formatos (single-thread).
-   `src/transcode_lz4/` / `transcode.sh`: Transcodificação LZ4/LZ4HC com suporte single-thread e multithread (arquitetura producer-consumer).
-   `src/decode.cpp` / `decode.sh`: Realiza a decodificação de um vídeo (FFmpeg). Para MJPEG com múltiplas threads (`-D > 1`), utiliza pipeline multithread automático com arquitetura producer-consumer (`src/decode_mjpeg_mt.cpp`).
-   `src/decode_mjpeg_mt.h` / `src/decode_mjpeg_mt.cpp`: Decodificação MJPEG multithread (producer-consumer com N instâncias do decoder FFmpeg).
-   `src/decode_lz4/` / `decode.sh`: Decodificação LZ4 multithread (arquitetura producer-consumer).
-   `src/frame_types.h`: Tipos compartilhados entre encode e decode (`FrameHeader`, `FrameItem`).
-   `src/queue.h` / `src/queue.c`: Fila genérica thread-safe com gerenciamento de ownership.
-   `dataset/compare.sh`: Compara a qualidade de diferentes arquivos de vídeo em relação a um arquivo de referência usando SSIM.
-   `build-ffmpeg.md`: Guia para compilar o FFmpeg com os codecs necessários.

## Pré-requisitos

-   **FFmpeg**: É necessário ter o FFmpeg instalado com os seguintes codecs habilitados: `libvvenc`, `libsvtav1`, `libvpx-vp9`, `libx264`, `libx265`. Consulte o guia `build-ffmpeg.md` para obter instruções de compilação.
-   **LZ4 e LZ4HC**: É necessário ter instalado os algoritmos de compactação/descompactação, o LZ4 e o LZ4HC. Para instalar use: `sudo apt install liblz4-dev`.
-   **Compilador C/C++**: É necessário ter um compilador C (como o `gcc`) para compilar `src/transcode.c` e um compilador C++ (como o `g++`) para compilar `src/decode.cpp` e os módulos em `src/decode_lz4/`.
-   **Arquivo de Vídeo YUV**: Para o script `dataset/generate.sh`, você precisará de um arquivo de vídeo bruto no formato YUV.

## Como Usar

### 1. Gerar Arquivos de Vídeo (`dataset/generate.sh`)

Este script codifica um arquivo de vídeo YUV bruto em vários formatos (VVC, AV1, VP9, AVC, HEVC).

**Uso:**

```bash
./dataset/generate.sh -i <caminho_para_arquivo_yuv> [-s <resolucao>]
```

-   `-i`: Caminho para o arquivo de entrada YUV.
-   `-s`: (Opcional) Resolução do vídeo de saída (ex: `1920x1080`). O padrão é `3840x2160`.

**Exemplo:**

```bash
./dataset/generate.sh -i original.yuv -s 1920x1080
```

### 2. Transcodificar um Vídeo (`transcode.sh`)

Este script compila e executa o programa `src/transcode.c` para transcodificar um vídeo para um formato específico, testando diferentes combinações de threads de entrada e saída.

#### Perfis de Threads

O benchmark utiliza **três perfis de configuração de threads** para avaliar diferentes cenários de uso:

| Perfil | Threads de Decodificação | Threads de Codificação | Descrição |
|--------|--------------------------|------------------------|-----------|
| `low_latency` | 1 | 1 | Focado em baixa latência, mínimo overhead de sincronização |
| `balanced` | 8 | 8 | Configuração equilibrada para uso geral |
| `high_throughput` | 16 | 16 | Focado em máxima vazão (throughput) |

**Nota:** Para codificadores LZ4 e LZ4HC, o perfil `low_latency` (1 thread codificadora) utiliza a implementação single-thread (`transcode_lz4_st.c`). Os perfis `balanced` e `high_throughput` utilizam a implementação multithread (`transcode_lz4_mt.c`) com arquitetura producer-consumer, onde uma thread decodifica com FFmpeg e múltiplas threads comprimem com LZ4/LZ4HC em paralelo. A seleção entre ST e MT é automática baseada no número de threads codificadoras (`-E`).

**Uso:**

```bash
./transcode.sh -i <video_de_entrada> -r <arquivo_de_resultados_csv> -e <encoder> [-o <diretorio_de_saida>] [-w]
```

-   `-i`: Caminho para o vídeo de entrada.
-   `-r`: Nome do arquivo CSV onde os resultados do benchmark serão salvos.
-   `-e`: Nome do encoder a ser utilizado (ex: `mjpeg`, `libsvtjpegxs`, `lz4`, `lz4hc`).
-   `-o`: (Opcional) Diretório onde os vídeos transcodificados serão salvos. O padrão é o diretório atual.
-   `-w`: (Opcional) Habilita escrita de arquivos de saída. **Padrão: desabilitado** (benchmark puro sem I/O de disco).

**Exemplo:**

```bash
# Benchmark sem escrita de arquivos (padrão - mais rápido)
./transcode.sh -i video.mp4 -r resultados_transcode.csv -e mjpeg -o ./output_transcode

# Com escrita de arquivos habilitada
./transcode.sh -i video.mp4 -r resultados_transcode.csv -e mjpeg -o ./output_transcode -w
```

### 3. Decodificar um Vídeo (`decode.sh`)

Este script compila e executa o programa `src/decode.cpp` (ou os módulos em `src/decode_lz4/` para descompactação LZ4) para decodificar um vídeo, testando o desempenho com diferentes números de threads. A decodificação LZ4 multithread utiliza arquitetura producer-consumer com fila genérica (`Queue`), onde uma thread lê frames do arquivo e múltiplas threads descompactam com LZ4 em paralelo.

**Decodificação MJPEG multithread**: Quando o vídeo de entrada utiliza codec MJPEG e `-D > 1`, o binário `decode` ativa automaticamente um pipeline multithread com arquitetura producer-consumer (`src/decode_mjpeg_mt.cpp`). O decoder MJPEG do FFmpeg é single-thread por natureza (não possui `AV_CODEC_CAP_FRAME_THREADS` nem `AV_CODEC_CAP_SLICE_THREADS`), então o paralelismo é implementado a nível de aplicação: múltiplas instâncias do decoder FFmpeg distribuem frames via Queue. Com `-D 1`, o caminho single-thread padrão é utilizado (sem overhead de Queue). Para codecs com threading nativo (H.264, HEVC, VP9 etc.), o FFmpeg gerencia o threading internamente.

#### Perfis de Threads

O benchmark de decodificação utiliza os **mesmos três perfis de configuração**:

| Perfil | Threads de Decodificação | Descrição |
|--------|--------------------------|-----------|
| `low_latency` | 1 | Focado em baixa latência |
| `balanced` | 8 | Configuração equilibrada para uso geral |
| `high_throughput` | 16 | Focado em máxima vazão (throughput) |

**Uso:**

```bash
./decode.sh -i <video_de_entrada> -r <arquivo_de_resultados_csv> [-o <diretorio_de_saida>] [-l <algoritmo_lz>] [-w]
```

-   `-i`: Caminho para o vídeo de entrada a ser decodificado.
-   `-r`: Nome do arquivo CSV onde os resultados do benchmark serão salvos.
-   `-o`: (Opcional) Diretório onde o vídeo decodificado (YUV) será salvo. O padrão é o diretório atual.
-   `-l`: (Opcional) Descompacta vídeo usando `lz4` ou `lz4hc` quando este foi compactado usando um desses métodos.
-   `-w`: (Opcional) Habilita escrita de arquivos de saída. **Padrão: desabilitado** (benchmark puro sem I/O de disco).

**Exemplo:**

```bash
# Benchmark sem escrita de arquivos (padrão - mais rápido)
./decode.sh -i video.mp4 -r resultados_decode.csv -o ./output_decode

# Com escrita de arquivos habilitada
./decode.sh -i video.mp4 -r resultados_decode.csv -o ./output_decode -w
```

### 4. Comparar Vídeos (`dataset/compare.sh`)

Este script compara um ou mais arquivos de vídeo com um arquivo de referência (geralmente o YUV original), calculando o tamanho do arquivo e a similaridade estrutural (SSIM).

**Uso:**

```bash
./dataset/compare.sh <resolucao> <arquivo_de_referencia> <codec2> <arquivo2> [<codec3> <arquivo3> ...]
```

-   `<resolucao>`: A resolução dos vídeos (ex: `3840x2160`).
-   `<arquivo_de_referencia>`: O arquivo de vídeo original (YUV) para comparação.
-   `<codecX>` e `<arquivoX>`: O codec e o caminho para cada vídeo a ser comparado com o de referência.

**Exemplo:**

```bash
./dataset/compare.sh 3840x2160 original.yuv h264 3840x2160_avc.mp4 hevc 3840x2160_hevc.mp4
```

## Compilação e Execução Individual

Além dos scripts `transcode.sh` e `decode.sh`, cada benchmark pode ser compilado e executado manualmente. Abaixo estão os comandos para cada binário.

> **Nota:** Antes de executar, exporte o caminho das bibliotecas: `export LD_LIBRARY_PATH="/usr/local/lib"`

### Transcodificação — Single-thread (`src/transcode.c`)

#### MJPEG (ou outros encoders FFmpeg)

```bash
gcc -O3 -Wall -Wno-unused-variable src/transcode.c -o transcode \
    -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm

./transcode -i video.mp4 -o output.mjpeg -e mjpeg -p balanced -D 8 -E 8

# Com escrita habilitada
./transcode -i video.mp4 -o output.mjpeg -e mjpeg -p balanced -D 8 -E 8 -w
```

**Parâmetros do binário `transcode`:**

| Parâmetro | Descrição |
|-----------|-----------|
| `-i` | Arquivo de vídeo de entrada |
| `-o` | Arquivo de saída |
| `-e` | Encoder: `mjpeg`, `libsvtjpegxs`, etc. |
| `-p` | Nome do perfil |
| `-D` | Threads decodificadoras (default: 0 = auto) |
| `-E` | Threads codificadoras (default: 0 = auto) |
| `-w` | Habilita escrita do arquivo de saída (default: desabilitado) |

### Transcodificação — LZ4/LZ4HC (`src/transcode_lz4/`)

```bash
gcc -O3 -Wall -Wno-unused-variable \
    src/transcode_lz4/transcode_lz4_main.c \
    src/transcode_lz4/transcode_lz4_st.c \
    src/transcode_lz4/transcode_lz4_mt.c \
    src/queue.c \
    src/cpu_stats.cpp \
    -o transcode_mt \
    -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -llz4 -lpthread

./transcode_mt -i video.mp4 -o output.lz4 -e lz4 -l 1 -p balanced -D 8 -E 8

# Com escrita habilitada
./transcode_mt -i video.mp4 -o output.lz4 -e lz4 -l 1 -p balanced -D 8 -E 8 -w
```

**Parâmetros do binário `transcode_mt`:**

| Parâmetro | Descrição |
|-----------|-----------|
| `-i` | Arquivo de vídeo de entrada |
| `-o` | Arquivo de saída |
| `-e` | Encoder: `lz4` ou `lz4hc` |
| `-l` | Nível de compressão (1–12; para LZ4 controla aceleração, para LZ4HC controla compressão) |
| `-p` | Nome do perfil |
| `-D` | Threads decodificadoras FFmpeg (default: 0 = auto) |
| `-E` | Threads codificadoras LZ4 (1 = single-thread, >1 = multithread) |
| `-w` | Habilita escrita do arquivo de saída (default: desabilitado) |

### Decodificação — FFmpeg (`src/decode.cpp` + MJPEG MT)

```bash
g++ -O3 -Wall -Wno-unused-variable -Wno-unused-function \
    src/decode.cpp src/decode_mjpeg_mt.cpp src/queue.c src/cpu_stats.cpp \
    -o decode -I/usr/local/include -L/usr/local/lib \
    -lavcodec -lavutil -lavformat -lm -lpthread

./decode -i video.mp4 -o output.yuv -p balanced -D 4

# Com escrita habilitada
./decode -i video.mp4 -o output.yuv -p balanced -D 4 -w
```

**Parâmetros do binário `decode`:**

| Parâmetro | Descrição |
|-----------|-----------|
| `-i` | Arquivo de vídeo de entrada |
| `-o` | Arquivo YUV de saída |
| `-p` | Nome do perfil |
| `-D` | Threads decodificadoras FFmpeg (default: 0 = auto) |
| `-w` | Habilita escrita do arquivo de saída (default: desabilitado) |

### Decodificação — LZ4 Multithread (`src/decode_lz4/`)

```bash
g++ -O3 -Wall -Wno-unused-variable -Wno-unused-function \
    src/decode_lz4/decode_lz4_main.cpp src/decode_lz4/decode_lz4_mt.cpp \
    src/decode_lz4/frame_reader.cpp src/decode_lz4/frame_decoder.cpp \
    src/decode_lz4/frame_writer.cpp src/decode_lz4/stats.cpp \
    src/queue.c src/cpu_stats.cpp \
    -o decode_lz4 -I/usr/local/include -L/usr/local/lib \
    -lavutil -lm -llz4 -lpthread

./decode_lz4 -i video.lz4 -o output.yuv -p balanced -D 8

# Com escrita habilitada
./decode_lz4 -i video.lz4 -o output.yuv -p balanced -D 8 -w
```

**Parâmetros do binário `decode_lz4`:**

| Parâmetro | Descrição |
|-----------|-----------|
| `-i` | Arquivo `.lz4` de entrada (funciona para LZ4 e LZ4HC) |
| `-o` | Arquivo YUV de saída |
| `-p` | Nome do perfil |
| `-D` | Threads decodificadoras (default: 1) |
| `-w` | Habilita escrita do arquivo de saída (default: desabilitado) |

### Opções de CLI para Threads

| Opção | Default | Descrição |
|-------|---------|-----------|
| `-D <N>` | 0 (auto) | Threads decodificadoras |
| `-E <N>` | 0 (auto) / 1 | Threads codificadoras |

## Formato dos Arquivos de Resultados

Os scripts de benchmark geram arquivos CSV com os seguintes formatos:

### Transcodificação (`transcode.sh`)

```
profile,decoder_threads,encoder_threads,type,time[,compression_level|acceleration]
```

- `profile`: Nome do perfil (`low_latency`, `balanced`, `high_throughput`)
- `decoder_threads`: Número de threads de decodificação
- `encoder_threads`: Número de threads de codificação
- `type`: Tipo de medição (`transcoding`, `total`, `fps`)
- `time`: Tempo em microssegundos (ou FPS para tipo `fps`)
- `compression_level`: Nível de compressão (apenas para LZ4HC)
- `acceleration`: Aceleração (apenas para LZ4)

### Decodificação (`decode.sh`)

```
profile,decoder_threads,encoder_threads,type,time
```

- `profile`: Nome do perfil (`low_latency`, `balanced`, `high_throughput`)
- `decoder_threads`: Número de threads de decodificação
- `encoder_threads`: Sempre 0 para decodificação
- `type`: Tipo de medição (`decoding`, `total`, `fps`)
- `time`: Tempo em microssegundos (ou FPS para tipo `fps`)



### 5. Verificar Corretude (SSIM)

**Importante**: A comparação SSIM deve ser feita exclusivamente entre arquivos YUV decodificados. Comparar um YUV decodificado diretamente com um vídeo comprimido (MP4) produz resultados incorretos devido a diferenças de range (YUVJ420P full-range vs YUV420P limited-range).

```bash
# Correto: YUV vs YUV (ambos decodificados)
ffmpeg -s 3840x2160 -pix_fmt yuv420p -i decoded/output1.yuv \
       -s 3840x2160 -pix_fmt yuv420p -i decoded/output2.yuv \
       -lavfi ssim -f null -
```

### 6. Analisar Resultados de Transcodificação (`results/transcoding/analyze_transcoding.py`)

Este script Python gera visualizações e análises dos resultados de benchmark de transcodificação.

**Uso:**

```bash
python3 results/transcoding/analyze_transcoding.py [arquivo_csv1.csv arquivo_csv2.csv ...]
```

**Gráficos gerados:**
- FPS por perfil de threads
- Tempo médio de frame por perfil
- Distribuição de tempo de frame (boxplot) por perfil
- Comparação de FPS por contagem de threads (decoder x encoder)

**Exemplo:**

```bash
python3 results/transcoding/analyze_transcoding.py resultados_transcode.csv
```

### 7. Analisar Resultados de Decodificação (`results/decoding/analyze_decoding.py`)

Este script Python gera visualizações e análises dos resultados de benchmark de decodificação.

**Uso:**

```bash
python3 results/decoding/analyze_decoding.py [arquivo_csv1.csv arquivo_csv2.csv ...]
```

**Gráficos gerados:**
- FPS por perfil de threads
- Tempo médio de frame por perfil
- Distribuição de tempo de frame (boxplot) por perfil
- Comparação de FPS por contagem de threads

**Exemplo:**

```bash
python3 results/decoding/analyze_decoding.py resultados_decode.csv
```
