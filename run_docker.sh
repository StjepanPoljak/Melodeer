#!/bin/bash

if ! [ -f "$HOME/.gitconfig" ]
then
	echo "(!) Please configure git."
	exit 3
fi

IMAGE_TAG=stjepan1986/melodeer
REPO_CHECK=$(docker images -q $IMAGE_TAG)

if [ -z "$REPO_CHECK" ]
then
	echo "(!) Please build the docker image first by running build-docker.sh script; or use: docker build <dockerfile_folder> -t stjepan1986/melodeer. If you have already built the image, but under different tag, use: docker tag <previous_tag> stjepan1986/melodeer"
	exit 1
fi

if [ "$#" -ne 0 ] && [ "$#" -ne 2 ] && [ "$#" -ne 4 ]
then
	echo "(!) Invalid number of arguments. This script accepts optional --mgui-dir and --media-dir arguments."
	exit 2
fi

MEDIA_DIR=
MDGUI_DIR=

PREV_ARG=

for ARG in "$@"
do
	if [ -z "$PREV_ARG" ]
	then
		if ! [ "$ARG" = "--mdgui-dir" ] && ! [ "$ARG" = "--media-dir" ]
		then
			echo "(!) Invalid argument '$ARG'. Valid arguments are --mdgui-dir and --media-dir"
			exit 3
		else
			PREV_ARG="$ARG"
		fi
	elif [ "$PREV_ARG" = "--mdgui-dir" ]
	then
		MDGUI_DIR="$ARG"
		unset PREV_ARG
	elif [ "$PREV_ARG" = "--media-dir" ]
	then
		MEDIA_DIR="$ARG"
		unset PREV_ARG
	fi
done

CURR_DIR="$(pwd)"
PARENT_DIR="${CURR_DIR%/*}"

if [ -z "$MDGUI_DIR" ]
then

	if ! [ -d "$PARENT_DIR/MelodeerGUI" ]
	then
		(cd ..; git clone "https://github.com/StjepanPoljak/MelodeerGUI")
	fi

	MDGUI_DIR="$PARENT_DIR/MelodeerGUI"
fi

docker run -ti --network host --device /dev/snd \
	-e PULSE_SERVER=unix:"${XDG_RUNTIME_DIR}"/pulse/native \
	-v "${XDG_RUNTIME_DIR}"/pulse/native:"${XDG_RUNTIME_DIR}"/pulse/native \
	-v "$HOME"/.config/pulse/cookie:/root/.config/pulse/cookie \
	--group-add $(getent group audio | cut -d: -f3) \
	-v "$CURR_DIR":/home/dev/Melodeer \
	-v "$MEDIA_DIR":/home/dev/Media \
	-v "$HOME"/.gitconfig:/home/dev/.gitconfig \
	-v "$MDGUI_DIR":/home/dev/MelodeerGUI \
	$IMAGE_TAG
