#!/bin/bash

if [ -z "$1" ]
then
	echo "(!) Please provide working folder as an argument. Second argument is optional folder to provide access to your music library."
	exit 1
fi

IMAGE_TAG=stjepan1986/melodeer
REPO_CHECK=$(docker images -q $IMAGE_TAG)

if [ -z "$REPO_CHECK" ]
then
	echo "(!) Please build the docker image first by running: docker build <dockerfile_folder> -t stjepan1986/melodeer. If you have already built the image, but under different tag, use: docker tag <previous_tag> stjepan1986/melodeer"
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
		$IMAGE_TAG
else
	docker run -ti \
		-v /dev/snd:/dev/snd \
		-v $HOME_PATH:/home/dev \
		-v $2:/media \
		--privileged \
		$IMAGE_TAG
fi
