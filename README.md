# Melodeer
Humble audio player based on OpenAL.

Melodeer is envisioned as a high-level cross-platform API dedicated to, not only playing, but also dynamically compressing and enabling further processing (e.g. with FFT) of audio files in lossless format (WAV and FLAC). There is also simple support for mp3 files (and may include more). It is still in development, and anybody willing to contribute is welcome to join in on the fun.

Melodeer should enable programmers to choose only formats in which they are interested. For example, if somebody wants a pure FLAC player library, he would need only mdcore.c and mdflac.c (along with .h files).

In order to build Melodeer, following libraries are needed (with respect to wanted modules):

* mdcore

  - OpenAL library (https://www.openal.org)

* mdflac

  - OGG library (https://github.com/gcp/libogg)
  - FLAC library (https://github.com/xiph/flac)

* mdlame

  - LAME library (http://lame.sourceforge.net)
