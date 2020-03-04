[![Build Status](https://travis-ci.org/StjepanPoljak/Melodeer.svg?branch=master)](https://travis-ci.org/StjepanPoljak/Melodeer)

# Melodeer

A humble audio player based on OpenAL.

Melodeer is envisioned as a high-level cross-platform API dedicated to, not only playing, but also dynamically compressing and enabling further processing (e.g. with FFT) of audio files in lossless format (WAV and FLAC). There is also support for mp3 files. It is still in development, and anybody willing to contribute is welcome to join in on the fun.

Melodeer should enable programmers to choose only formats in which they are interested. For example, if somebody wants a pure FLAC player library, he would need only `mdcore.c` and `mdflac.c` (along with `.h` files).

## Build

In order to build Melodeer, following libraries are needed (customizable by tinkering CMake):

* [OpenAL](https://github.com/kcat/openal-soft)

* [FLAC](https://github.com/xiph/flac)

* [mpg123](https://sourceforge.net/projects/mpg123/files/)

On Ubuntu, to install these dependencies, just do:

```shell
sudo apt install libopenal-dev libmpg123-dev libflac-dev
```

Then, to build and install both the executable and the shared library, do:

```shell
cmake .
make
```
