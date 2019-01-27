#!/bin/bash

if [ -z "$1" ]
then
	echo "(!) Please provide working folder as an argument. Second argument is optional folder to provide access to your music library."
	exit 1
fi

REPO_CHECK=$(docker images -q melodeer)

if [ -z "$REPO_CHECK" ]
then
	echo "(!) Please build the docker image first by running: docker build <dockerfile_folder> -t melodeer"
	exit 2
fi

HOME_PATH=$1

(cd $HOME_PATH; git clone https://www.github.com/StjepanPoljak/Melodeer; git clone https://www.github.com/StjepanPoljak/MelodeerGUI)


if [ -z "$2" ]
then
	docker run -ti \
		-v /dev/snd:/dev/snd \
		-v $HOME_PATH:/home/dev \
		--privileged \
		melodeer
else
	docker run -ti \
		-v /dev/snd:/dev/snd \
		-v $HOME_PATH:/home/dev \
		-v $2:/media \
		--privileged \
		melodeer
fi
