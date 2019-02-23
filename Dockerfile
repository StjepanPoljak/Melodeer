FROM debian

RUN apt-get -y update && apt-get -y install git make sudo gcc libopenal-dev \
	libmpg123-dev libflac-dev gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 wine64

#RUN apt-get -y install autoconf automake autopoint bash bison bzip2 flex g++ \
#    g++-multilib gettext gperf intltool libc6-dev-i386 libgdk-pixbuf2.0-dev \
#    libltdl-dev libssl-dev libtool-bin libxml-parser-perl lzip openssl \
#    p7zip-full patch perl pkg-config python ruby sed unzip wget xz-utils

RUN rm -rf /var/cache/apt/*

RUN useradd -m dev && echo "dev:dev" | chpasswd && adduser dev sudo

USER dev
WORKDIR /home/dev

CMD bash
