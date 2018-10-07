#!/bin/bash

if [ "$EUID" -ne 0 ]
then
	echo "Please run as root ($1)."
	exit 1
fi

exit 0
