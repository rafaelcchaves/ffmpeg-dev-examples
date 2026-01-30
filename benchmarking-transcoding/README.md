# Benchmarking de Transcodificação de Vídeo

Este projeto contém um conjunto de scripts para realizar benchmarking de transcodificação e decodificação de vídeo utilizando FFmpeg e compilando programas em C.

## Estrutura do Projeto

-   `dataset/generate.sh`: Gera arquivos de vídeo codificados a partir de um arquivo YUV bruto.
-   `transcode.c` / `transcode.sh`: Realiza a transcodificação de um vídeo para diferentes formatos.
-   `decode.c` / `decode.sh`: Realiza a decodificação de um vídeo.
-   `compare.sh`: Compara a qualidade de diferentes arquivos de vídeo em relação a um arquivo de referência usando SSIM.
-   `build-ffmpeg.md`: Guia para compilar o FFmpeg com os codecs necessários.

## Pré-requisitos

-   **FFmpeg**: É necessário ter o FFmpeg instalado com os seguintes codecs habilitados: `libvvenc`, `libsvtav1`, `libvpx-vp9`, `libx264`, `libx265`. Consulte o guia `build-ffmpeg.md` para obter instruções de compilação.
-   **LZ4 e LZ4HC**: É necessário ter instalado os algoritmos de compactação/descompactação, o LZ4 e o LZ4HC. Para instalar use: `sudo apt install liblz4-dev`.
-   **Compilador C**: É necessário ter um compilador C (como o `gcc`) para compilar os programas `transcode.c` e `decode.c`.
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

Este script compila e executa o programa `transcode.c` para transcodificar um vídeo para um formato específico, testando diferentes combinações de threads de entrada e saída.

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

Este script compila e executa o programa `decode.c` para decodificar um vídeo, testando o desempenho com diferentes números de threads.

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

### 4. Comparar Vídeos (`compare.sh`)

Este script compara um ou mais arquivos de vídeo com um arquivo de referência (geralmente o YUV original), calculando o tamanho do arquivo e a similaridade estrutural (SSIM).

**Uso:**

```bash
./compare.sh <resolucao> <arquivo_de_referencia> <codec2> <arquivo2> [<codec3> <arquivo3> ...]
```

-   `<resolucao>`: A resolução dos vídeos (ex: `3840x2160`).
-   `<arquivo_de_referencia>`: O arquivo de vídeo original (YUV) para comparação.
-   `<codecX>` e `<arquivoX>`: O codec e o caminho para cada vídeo a ser comparado com o de referência.

**Exemplo:**

```bash
./compare.sh 3840x2160 original.yuv h264 3840x2160_avc.mp4 hevc 3840x2160_hevc.mp4
```
