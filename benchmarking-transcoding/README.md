# Benchmark de Transcodificação de Vídeo

Este projeto fornece um conjunto de ferramentas para avaliar (benchmark) o desempenho de um transcodificador de vídeo simples.  
Ele automatiza o processo de compilação e execução de um transcodificador em C com várias configurações de threads, coleta dados de desempenho e gera uma análise visual dos resultados.

---

## Descrição dos Arquivos

transcode.c** — Código-fonte de um transcodificador simples que transcodifica streams de vídeo de vários formatos de contêiner (por exemplo, MP4) para um formato de saída especificado, usando as bibliotecas FFmpeg e detectando automaticamente o codec de entrada.
- **`transcode.sh`** — Script de shell que automatiza o processo de benchmarking. Ele compila e executa o `transcode.c` várias vezes com diferentes números de threads de codificação e decodificação e salva os resultados em um arquivo CSV.  
- **`compare.sh`** — Script de shell que compara a qualidade de vídeo e o tamanho de arquivo de diferentes codecs.  
- **`analyse.py`** — Script Python que lê os dados do CSV gerado pelo benchmark e cria gráficos de mapa de calor (heatmap) para visualizar o desempenho com base nas configurações de thread.

---

## Pré-requisitos

### 1. Compilador C e Bibliotecas FFmpeg

Você precisará do `gcc` e das bibliotecas de desenvolvimento do FFmpeg (`libavcodec` e `libavutil`).

Em sistemas baseados em Debian/Ubuntu:

```bash
sudo apt-get update && sudo apt-get install build-essential libavcodec-dev libavutil-dev libavformat-dev libm-dev
```

### 2. Ambiente Python

Você precisará do Python 3 e dos seguintes pacotes:

```bash
pip install pandas seaborn matplotlib
```

---

## Como Usar

### Passo 1: Executar o Script de Benchmark

Primeiro, torne o script `transcode.sh` executável:

```bash
chmod +x transcode.sh
```

Em seguida, execute o script com as flags apropriadas.
O script compila e executa o código C para 36 combinações diferentes de threads.

Sintaxe:

```bash
./transcode.sh -i <video-de-entrada> -r <Arquivo-CSV-de-Saida> -e <encoder> [-o <diretorio-de-saida>]
```

**Exemplo:**

```bash
./transcode.sh -i input.mp4 -r benchmarking.csv -e mjpeg -o out
```

Isso executará o benchmark e salvará os resultados no arquivo `benchmarking.csv` e os vídeos transcodificados no diretório `out`.

### Passo 2: Comparar os Vídeos

O script `compare.sh` permite comparar a qualidade (usando SSIM) e o tamanho dos arquivos de vídeo transcodificados com diferentes codecs.

**Sintaxe:**

```bash
./compare.sh <codec-de-referencia> <arquivo-de-referencia> <codec2> <arquivo2> [<codec3> <arquivo3> ...]
```

**Exemplo:**

```bash
./compare.sh h264 h264.raw mjpeg out/0_0.mjpeg jpegxs out/0_0.jpegxs
```

### Passo 3: Analisar os Resultados

Após o término da execução, use o script `analyse.py` para visualizar os resultados:

```bash
python3 analyse.py benchmarking.csv
```

O script processará os dados e exibirá mapas de calor mostrando o tempo de execução em relação ao número de threads de codificação e decodificação.

---

## Saída Esperada

- Arquivo CSV com os resultados do benchmark (`benchmarking.csv`)
- Vídeos transcodificados no diretório especificado
- Gráficos de mapa de calor exibindo o desempenho de acordo com as configurações de threads