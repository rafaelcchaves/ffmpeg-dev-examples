# Benchmarking de Transcodificação de Vídeo

Este projeto contém um conjunto de scripts para realizar benchmarking de transcodificação e decodificação de vídeo utilizando FFmpeg e compilando programas em C.

## Estrutura do Projeto

-   `dataset/generate.sh`: Gera arquivos de vídeo codificados a partir de um arquivo YUV bruto.
-   `src/transcode.c` / `transcode.sh`: Realiza a transcodificação de um vídeo para diferentes formatos.
-   `src/decode.cpp` / `decode.sh`: Realiza a decodificação de um vídeo.
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

**Nota:** Para codificadores LZ4 e LZ4HC, a codificação utiliza apenas 1 thread em todos os perfis, pois não há suporte a multi-threading implementado para esses codecs.

**Uso:**

```bash
./transcode.sh -i <video_de_entrada> -r <arquivo_de_resultados_csv> -e <encoder> [-o <diretorio_de_saida>]
```

-   `-i`: Caminho para o vídeo de entrada.
-   `-r`: Nome do arquivo CSV onde os resultados do benchmark serão salvos.
-   `-e`: Nome do encoder a ser utilizado (ex: `mjpeg`, `libsvtjpegxs`, `lz4`, `lz4hc`).
-   `-o`: (Opcional) Diretório onde os vídeos transcodificados serão salvos. O padrão é o diretório atual.

**Exemplo:**

```bash
./transcode.sh -i video.mp4 -r resultados_transcode.csv -e mjpeg -o ./output_transcode
```

### 3. Decodificar um Vídeo (`decode.sh`)

Este script compila e executa o programa `src/decode.cpp` (ou os módulos em `src/decode_lz4/` para descompactação LZ4) para decodificar um vídeo, testando o desempenho com diferentes números de threads.

#### Perfis de Threads

O benchmark de decodificação utiliza os **mesmos três perfis de configuração**:

| Perfil | Threads de Decodificação | Descrição |
|--------|--------------------------|-----------|
| `low_latency` | 1 | Focado em baixa latência |
| `balanced` | 8 | Configuração equilibrada para uso geral |
| `high_throughput` | 16 | Focado em máxima vazão (throughput) |

**Uso:**

```bash
./decode.sh -i <video_de_entrada> -r <arquivo_de_resultados_csv> [-o <diretorio_de_saida>] [-l <algoritmo_lz>]
```

-   `-i`: Caminho para o vídeo de entrada a ser decodificado.
-   `-r`: Nome do arquivo CSV onde os resultados do benchmark serão salvos.
-   `-o`: (Opcional) Diretório onde o vídeo decodificado (YUV) será salvo. O padrão é o diretório atual.
-   `-l`: (Opcional) Descompacta vídeo usando `lz4` ou `lz4hc` quando este foi compactado usando um desses métodos.

**Exemplo:**

```bash
./decode.sh -i video.mp4 -r resultados_decode.csv -o ./output_decode
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

## Formato dos Arquivos de Resultados

Os scripts de benchmark geram arquivos CSV com os seguintes formatos:

### Transcodificação (`transcode.sh`)

```
profile,threads_in,threads_out,type,time[,compression_level|acceleration]
```

- `profile`: Nome do perfil (`low_latency`, `balanced`, `high_throughput`)
- `threads_in`: Número de threads de decodificação
- `threads_out`: Número de threads de codificação
- `type`: Tipo de medição (`transcoding`, `total`, `fps`)
- `time`: Tempo em microssegundos (ou FPS para tipo `fps`)
- `compression_level`: Nível de compressão (apenas para LZ4HC)
- `acceleration`: Aceleração (apenas para LZ4)

### Decodificação (`decode.sh`)

```
profile,threads_in,threads_out,type,time
```

- `profile`: Nome do perfil (`low_latency`, `balanced`, `high_throughput`)
- `threads_in`: Número de threads de decodificação
- `threads_out`: Sempre 0 para decodificação
- `type`: Tipo de medição (`decoding`, `total`, `fps`)
- `time`: Tempo em microssegundos (ou FPS para tipo `fps`)



### 5. Analisar Resultados de Transcodificação (`results/transcoding/analyze_transcoding.py`)

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

### 6. Analisar Resultados de Decodificação (`results/decoding/analyze_decoding.py`)

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
