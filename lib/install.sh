#!/bin/bash

# run with sudo (sudo ./install.sh) to install precompiled Melodeer library

UNAMEOUT="$(uname -s)"

case "${UNAMEOUT}" in
    Linux*)     MACHINE=Linux;;
    Darwin*)    MACHINE=Mac;;
    CYGWIN*)    MACHINE=Cygwin;;
    MINGW*)     MACHINE=MinGw;;
    *)          MACHINE="UNKNOWN:${unameOut}"
esac

if [ -f "$MACHINE/libmelodeer.so" ]; then cp "$MACHINE/libmelodeer.so" "/usr/local/lib/"; chmod 0755 "/usr/local/lib/libmelodeer.so"; echo "Copied library to /usr/local/lib."; fi
if [ -d /lib64 ]; then cp "$MACHINE/libmelodeer.so" "/lib64/"; chmod 0755 "/lib64/libmelodeer.so"; echo "Copied library to /lib64."; fi
if [ "$MACHINE" = "Linux" ]; then echo "Running ldconfig."; ldconfig; fi

exit
