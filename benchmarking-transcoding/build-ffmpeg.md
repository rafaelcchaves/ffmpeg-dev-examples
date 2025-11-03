# Compilando FFmpeg com Codecs Adicionais

Este guia fornece os passos para compilar o FFmpeg com vários codecs adicionais.

## Pré-requisitos

Certifique-se de que você tem as ferramentas de compilação necessárias instaladas no seu sistema. Para sistemas baseados em Debian, você pode instalá-las com:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake nasm libgnutls28-dev
```

## Instalação dos Codecs

### SVT-JPEG-XS

[SVT-JPEG-XS](https://github.com/OpenVisualCloud/SVT-JPEG-XS) é um codec JPEG XS.

```bash
git clone https://github.com/OpenVisualCloud/SVT-JPEG-XS.git
cd SVT-JPEG-XS/Build/linux
./build.sh install --prefix /usr/local/ 
cd ..
# Aplicar patch no FFmpeg
cd <caminho-para-ffmpeg>
cp <caminho-para-svt-jpeg>/ffmpeg-plugin/libsvtjpegxs* ./libavcodec/ 
git am --whitespace=fix <caminho-para-svt-jpeg>/ffmpeg-plugin/7.1/*.patch
```

### SVT-AV1

[SVT-AV1](https://gitlab.com/AOMediaCodec/SVT-AV1) é um codificador AV1.

```bash
git clone --depth=1 https://gitlab.com/AOMediaCodec/SVT-AV1.git
cd SVT-AV1
git fetch -t
git checkout v1.8.0
mkdir build && cd build
cmake .. -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/
make -j $(nproc)
sudo make install
```

### dav1d

[dav1d](httpss://code.videolan.org/videolan/dav1d) é um decodificador AV1.

```bash
sudo apt install libdav1d-dev
```

### AVC (x264)

[x264](https://www.videolan.org/developers/x264.html) é um codificador H.264/AVC.

```bash
sudo apt install nasm libx264-dev
```

### HEVC (x265)

[x265](https://www.videolan.org/developers/x265.html) é um codificador H.265/HEVC.

```bash
sudo apt install libx265-dev libnuma-dev
```

### VP9

```bash
sudo apt install libvpx-dev
```

### VVC (vvenc e vvdec)

[vvenc](https://github.com/fraunhoferhhi/vvenc) e [vvdec](https://github.com/fraunhoferhhi/vvdec) são um codificador e decodificador VVC.

#### vvenc

```bash
git clone https://github.com/fraunhoferhhi/vvenc.git
cd vvenc
sudo make install install-prefix=/usr/local 
```

#### vvdec

```bash
git clone https://github.com/fraunhoferhhi/vvdec.git
cd vvdec
sudo make install install-prefix=/usr/local 
```

##### Aplicar patch no FFmpeg para suporte ao vvdec

```bash
wget -O ~/libvvdec.patch https://raw.githubusercontent.com/wiki/fraunhoferhhi/vvdec/data/patch/v7-0001-avcodec-add-external-dec-libvvdec-for-H266-VVC.patch
cd <caminho-para-ffmpeg>
patch -p1 < ~/libvvdec.patch
```

## Instalação do FFmpeg

### Baixar o FFmpeg

```bash
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg
cd ffmpeg
git checkout n7.1
```

### Configurar e Compilar o FFmpeg

```bash
./configure \
  --prefix=/usr/local \
  --bindir=/usr/local/bin \
  --enable-shared \
  --extra-cflags="-I/usr/local/include" \
  --enable-gpl \
  --enable-gnutls \
  --enable-libsvtav1 \
  --enable-libdav1d \
  --enable-libsvtjpegxs \
  --enable-libvpx \
  --enable-libx264 \
  --enable-libx265 \
  --enable-libvvenc \
  --enable-libvvdec

make -j $(nproc)
sudo make install
sudo ldconfig
```
