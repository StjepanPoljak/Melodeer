UNAMEOUT="$(uname -s)"

case "${UNAMEOUT}" in
    Linux*)     MACHINE=Linux;;
    Darwin*)    MACHINE=Mac;;
    CYGWIN*)    MACHINE=Cygwin;;
    MINGW*)     MACHINE=MinGw;;
    *)          MACHINE="UNKNOWN:${unameOut}"
esac

if [ -f "melodeer" ]
then

	if [ ! -d "exec" ]
	then
		mkdir "exec"
	fi

	if [ ! -d "exec/$MACHINE" ]
	then
		mkdir "exec/$MACHINE"
	fi

	cp "melodeer" "exec/$MACHINE/"
fi

if [ -f "/usr/local/lib/libmelodeer.so" ]
then
	if [ ! -d "lib" ]
	then
		mkdir "lib"
	fi

	if [ ! -d "lib/$MACHINE" ]
	then
		mkdir "lib/$MACHINE"
	fi

	cp "/usr/local/lib/libmelodeer.so" "lib/$MACHINE/"
fi
