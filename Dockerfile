FROM debian

RUN apt-get update

RUN apt-get -y install git make gcc

RUN apt-get -y install libopenal-dev libmpg123-dev libflac-dev

RUN apt-get -y install sudo

RUN useradd -ms /bin/bash dev -G sudo

RUN usermod dev -G audio

RUN echo dev:dev | chpasswd

RUN echo 'dev ALL=(ALL) ALL' >> /etc/sudoers

RUN rm -rf /var/cache/apt/*

USER dev
WORKDIR /home/dev

CMD bash
