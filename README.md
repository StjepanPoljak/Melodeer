# Melodeer

[![Build Status](https://travis-ci.org/StjepanPoljak/Melodeer.svg?branch=master)](https://travis-ci.org/StjepanPoljak/Melodeer)

Melodeer is a humble audio player library. The latest version supports OpenAL and Pulseaudio as drivers and a FLAC decoder. There is also a dummy driver and a dummy decoder that make tests very easy, especially with Valgrind. It is still in development, but a stable version used by [MelodeerGUI](https://github.com/StjepanPoljak/MelodeerGUI) is available under tag `melodeer-v1.0.0`. As this is the only codebase depending on Melodeer, and as it will also require rework, improvements done in Melodeer v2.0.0 won't be backward compatible.

## Build

In order to build Melodeer, following libraries are needed:

* [OpenAL](https://github.com/kcat/openal-soft)

* [Pulseaudio](https://gitlab.freedesktop.org/pulseaudio/pulseaudio)

* [FLAC](https://github.com/xiph/flac)

* [mpg123](https://sourceforge.net/projects/mpg123/files/)

On Ubuntu, to install these dependencies, just do:

```shell
sudo apt install libopenal-dev libmpg123-dev libflac-dev libpulse-dev
```

Then, to build both the executable and the shared library, do:

```shell
cmake .
make
```
