# Benchmark de Transcodificação de Vídeo

Este projeto fornece um conjunto de ferramentas para avaliar (benchmark) o desempenho de um transcodificador de vídeo simples.  
Ele automatiza o processo de compilação e execução de um transcodificador em C com várias configurações de threads, coleta dados de desempenho e gera uma análise visual dos resultados.

---

## Descrição dos Arquivos

- **`transcode.c`** — Código-fonte de um transcodificador simples que converte streams de vídeo H.264 para MJPEG usando as bibliotecas FFmpeg.  
- **`transcode.sh`** — Script de shell que automatiza o processo de benchmarking. Ele compila e executa o `transcode.c` várias vezes com diferentes números de threads de codificação e decodificação e salva os resultados em um arquivo CSV.  
- **`analyse.py`** — Script Python que lê os dados do CSV gerado pelo benchmark e cria gráficos de mapa de calor (heatmap) para visualizar o desempenho com base nas configurações de thread.

---

## Pré-requisitos

### 1. Compilador C e Bibliotecas FFmpeg

Você precisará do `gcc` e das bibliotecas de desenvolvimento do FFmpeg (`libavcodec` e `libavutil`).

Em sistemas baseados em Debian/Ubuntu:

```bash
sudo apt-get update && sudo apt-get install build-essential libavcodec-dev libavutil-dev
```

### 2. Ambiente Python

Você precisará do Python 3 e dos seguintes pacotes:

```bash
pip install pandas seaborn matplotlib
```

---

## Como Usar

### Passo 1: Compilar o transcodificador (Opcional)

O script de benchmark compila o código automaticamente. No entanto, se você quiser compilar o transcodificador manualmente, pode usar o seguinte comando:

```bash
gcc transcode.c -o transcode -lavcodec -lavutil
```

Para executar o transcodificador manualmente:

```bash
./transcode -i <video-de-entrada> -s <Largura>x<Altura> -f <Fps> -o <video-de-saida> -e <encoder>
```

### Passo 2: Executar o Script de Benchmark

Primeiro, torne o script `transcode.sh` executável:

```bash
chmod +x transcode.sh
```

Em seguida, execute o script com as flags apropriadas.
O script compila e executa o código C para 36 combinações diferentes de threads.

**Sintaxe:**

```bash
./transcode.sh -i <video-de-entrada> -s <Largura>x<Altura> -f <Fps> -o <Arquivo-CSV-de-Saida> -e <encoder>
```

**Exemplo:**

```bash
./transcode.sh -i h264.raw -s 1920x1080 -f 30 -o benchmarking.csv -e libsvtjpegxs
```

Isso executará o benchmark e salvará os resultados no arquivo `benchmarking.csv`.

---

### Passo 2: Analisar os Resultados

Após o término da execução, use o script `analyse.py` para visualizar os resultados:

```bash
python3 analyse.py benchmarking.csv
```

O script processará os dados e exibirá mapas de calor mostrando o tempo de execução em relação ao número de threads de codificação e decodificação.

---

## Saída Esperada

- Arquivo CSV com os resultados do benchmark (`benchmarking.csv`)
- Gráficos de mapa de calor exibindo o desempenho de acordo com as configurações de threads


