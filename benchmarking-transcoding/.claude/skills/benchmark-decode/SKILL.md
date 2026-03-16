---
name: benchmark-decode
description: Executar benchmarks de decodificação de vídeo com FFmpeg ou LZ4. Use para medir desempenho de decodificação e analisar resultados.
---

# Benchmark de Decodificação

## Visão Geral

O script `decode.sh` executa benchmarks de decodificação de vídeo em dois modos:
1. **FFmpeg** - Decodificação de vídeos padrão (H.264, HEVC, etc.)
2. **LZ4** - Decodificação de arquivos comprimidos com LZ4

### Arquivos Relacionados

| Arquivo | Descrição |
|---------|-----------|
| `decode.sh` | Script principal de orquestração |
| `src/decode.cpp` | Decodificação FFmpeg (H.264, HEVC, etc.) |
| `src/decode_lz4/*` | Decodificação LZ4 multithread |
| `results/decoding/analyze_decoding.py` | Análise e visualização dos resultados |

---

## Uso Básico

### Modo FFmpeg (vídeos padrão)

```bash
./decode.sh -i <input-video> -r <csv-result> [-o <output-dir>] [-w]
```

### Modo LZ4 (arquivos comprimidos)

```bash
./decode.sh -i <input.lz4> -r <csv-result> -l lz4 [-o <output-dir>] [-p <profile>] [-w]
```

### Parâmetros

| Parâmetro | Obrigatório | Descrição |
|-----------|-------------|-----------|
| `-i` | Sim | Arquivo de vídeo de entrada |
| `-r` | Sim | Arquivo CSV para salvar resultados |
| `-o` | Não | Diretório de saída (default: `.`) |
| `-l` | Não | Algoritmo LZ (`lz4` ou `lz4hc`) |
| `-p` | Não | Perfil específico (default: todos) |
| `-w` | Não | Habilita escrita de arquivos de saída (default: desabilitado) |

### Flag de Escrita (`-w`)

Por padrão, os benchmarks são executados **sem escrita de arquivos** para medir apenas o desempenho de CPU:

```bash
# Benchmark puro (padrão) - sem I/O de disco
./decode.sh -i video.mp4 -r results.csv -o output/
# Arquivos YUV de saída terão tamanho 0 ou não existirão
```

Para habilitar a escrita de arquivos:

```bash
# Com escrita habilitada
./decode.sh -i video.mp4 -r results.csv -o output/ -w
# Arquivos YUV serão gerados normalmente
```

---

## Modos de Operação

### 1. Modo FFmpeg (`src/decode.cpp`)

Decodifica vídeos em formatos padrão usando FFmpeg/libav.

**Codecs Suportados:**
- H.264/AVC
- HEVC/H.265
- AV1
- Outros codecs do FFmpeg

**Características:**
- Usa `avcodec_send_packet()` / `avcodec_receive_frame()`
- Mede latência por frame (tempo desde `pkt_dts`)
- Suporte a threading via `thread_count`

### 2. Modo LZ4 (`src/decode_lz4/*`)

Decodifica arquivos comprimidos com LZ4 usando arquitetura multithread.

**Arquitetura:**

```
┌──────────────┐     ┌─────────────────┐     ┌──────────────┐
│   Produtor   │ ──► │ Buffer Compart. │ ──► │ Consumidores │
│ (Leitura)    │     │  (Semaforos)    │     │ (Decodific.) │
└──────────────┘     └─────────────────┘     └──────────────┘
       │                                            │
       ▼                                            ▼
  Frame Header                              Escrita Sequencial
  Dados comprimidos                              (YUV)
```

**Módulos:**

| Arquivo | Função |
|---------|--------|
| `decode_lz4_main.cpp` | Entry point, parsing de argumentos |
| `decode_lz4_mt.cpp` | Sistema multithread (produtor/consumidor) |
| `frame_reader.cpp` | Leitura de frames do arquivo |
| `frame_decoder.cpp` | Descompressão LZ4 de frames |
| `frame_writer.cpp` | Escrita de frames YUV |
| `stats.cpp` | Coleta e impressão de estatísticas |
| `decode_lz4.h` | Headers e estruturas compartilhadas |

---

## Perfis de Threads

### Modo FFmpeg

| Perfil | Threads | Uso |
|--------|---------|-----|
| `low_latency` | 1 | Menor latência |
| `balanced` | 8 | Equilíbrio |
| `high_throughput` | 16 | Máximo throughput |

### Modo LZ4

| Perfil | Threads | Uso |
|--------|---------|-----|
| `low_latency` | 1 | Menor latência |
| `balanced` | 4 | Equilíbrio |
| `high_throughput` | 8 | Máximo throughput |

**Nota:** LZ4 usa menos threads pois o gargalo é a CPU, não I/O.

---

## Formato do CSV de Saída

### Modo FFmpeg

```csv
profile,threads_in,threads_out,type,time
low_latency,1,0,decoding,1234
low_latency,1,0,total,567890
low_latency,1,0,fps,60.5
```

### Modo LZ4

```csv
profile,threads_in,threads_out,type,time
low_latency,1,0,decoding,1234
low_latency,1,0,total,567890
low_latency,1,0,fps,60.5
```

### Colunas

| Coluna | Descrição |
|--------|-----------|
| `profile` | Nome do perfil (`low_latency`, `balanced`, `high_throughput`) |
| `threads_in` | Número de threads de decodificação |
| `threads_out` | Sempre 0 (decodificação não tem saída codificada) |
| `type` | Tipo de métrica (`decoding`, `total`, `fps`) |
| `time` | Tempo em microssegundos ou FPS |

**Tipos de Métrica:**
- `decoding` - Tempo de decodificação de um frame individual
- `total` - Tempo total de processamento
- `fps` - Frames por segundo (throughput)

---

## Análise de Resultados

### Usando analyze_decoding.py

```bash
python3 results/decoding/analyze_decoding.py resultado1.csv resultado2.csv ...
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

## Workflow Completo

### 1. Transcodificar → Decodificar → Analisar

```bash
# Passo 1: Transcodificar vídeo para LZ4
./transcode.sh \
  -i dataset/videos/foreman.mp4 \
  -r results/transcoding/h264-lz4.csv \
  -e lz4 \
  -o output/

# Passo 2: Decodificar arquivo LZ4
./decode.sh \
  -i output/high_throughput.lz4 \
  -r results/decoding/lz4-decode.csv \
  -l lz4 \
  -o output/yuv/

# Passo 3: Analisar resultados
python3 results/decoding/analyze_decoding.py results/decoding/lz4-decode.csv
```

### 2. Comparar Codecs de Vídeo

```bash
# Decodificar H.264
./decode.sh \
  -i dataset/videos/foreman_h264.mp4 \
  -r results/decoding/h264.csv \
  -o output/

# Decodificar HEVC
./decode.sh \
  -i dataset/videos/foreman_hevc.mp4 \
  -r results/decoding/hevc.csv \
  -o output/

# Analisar ambos
python3 results/decoding/analyze_decoding.py \
  results/decoding/h264.csv \
  results/decoding/hevc.csv
```

---

## Exemplos Práticos

### Exemplo 1: Decodificação FFmpeg Básica

```bash
./decode.sh \
  -i dataset/videos/foreman.mp4 \
  -r results/decoding/foreman.csv \
  -o output/yuv/
```

### Exemplo 2: Decodificação LZ4 com Perfil Específico

```bash
./decode.sh \
  -i output/high_throughput.lz4 \
  -r results/decoding/lz4-high.csv \
  -l lz4 \
  -p high_throughput \
  -o output/yuv/
```

### Exemplo 3: Decodificação LZ4HC

```bash
./decode.sh \
  -i output/balanced.lz4hc \
  -r results/decoding/lz4hc.csv \
  -l lz4hc \
  -o output/yuv/
```

### Exemplo 4: Análise Comparativa

```bash
python3 results/decoding/analyze_decoding.py \
  results/decoding/h264.csv \
  results/decoding/hevc.csv \
  results/decoding/lz4.csv
```

---

## Arquitetura do Código

### decode.cpp (FFmpeg)

**Fluxo:**
```
avformat_open_input() → av_read_frame() → avcodec_send_packet()
                                              ↓
File YUV ← av_frame_unref() ← avcodec_receive_frame()
```

**Funções Principais:**

| Função | Descrição |
|--------|-----------|
| `main()` | Inicialização, loop de leitura, estatísticas |
| `decode()` | Envia pacote e recebe frames decodificados |

### decode_lz4/* (Sistema Multithread)

**Estruturas Principais:**

| Estrutura | Descrição |
|-----------|-----------|
| `FrameHeader` | Header do frame (tamanhos, dimensões, pixel format) |
| `DecodeContext` | Buffers de decodificação por thread |
| `SharedFrameInfo` | Informações do frame no buffer compartilhado |
| `ThreadContext` | Contexto privativo de cada thread |

**Sincronização:**

| Primitiva | Uso |
|-----------|-----|
| `sem_t g_sem_empty` | Buffer vazio (produtor pode escrever) |
| `sem_t g_sem_full` | Buffer com dados (consumidor pode ler) |
| `pthread_mutex_t g_write_mutex` | Escrita sequencial no output |
| `pthread_cond_t g_write_cond` | Ordenação de escrita |

**Funções Multithread:**

| Função | Descrição |
|--------|-----------|
| `mt_producer_thread()` | Lê frames do arquivo e coloca no buffer |
| `mt_decoder_thread()` | Retira frames, decodifica e escreve |
| `mt_decode_main()` | Coordena todo o sistema |

---

## Debug Mode

O sistema LZ4 suporta modo debug para métricas detalhadas:

```bash
# Compilar com debug
g++ -DDEBUG -O3 src/decode_lz4/*.cpp ...

# Executar (saída detalhada em stderr)
./decode_lz4 -i input.lz4 -o output.yuv -p debug -t 4
```

---

## Verificação de Integridade

Para verificar se a decodificação está correta:

```bash
# Comparar com vídeo original (usando FFmpeg para converter YUV)
ffmpeg -s 352x288 -pix_fmt yuv420p -i output/low_latency.yuv \
       -i dataset/videos/foreman.mp4 \
       -lavfi psnr -f null -
```
