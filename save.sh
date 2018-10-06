#!/bin/bash

# saves current compilation state to exec and lib

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

	if [ -f "lib/install.sh" ]
	then
		rm "lib/install.sh"
	fi	

	touch "lib/install.sh"

	echo '#!/bin/bash' >> "lib/install.sh"
	echo "" >> "lib/install.sh"
	echo '# run with sudo (sudo ./install.sh) to install precompiled Melodeer library' >> "lib/install.sh"
	echo "" >> "lib/install.sh"	
	echo 'UNAMEOUT="$(uname -s)"' >> "lib/install.sh"
	echo "" >> "lib/install.sh"
	echo 'case "${UNAMEOUT}" in' >> "lib/install.sh"
    	echo '    Linux*)     MACHINE=Linux;;' >> "lib/install.sh"
    	echo '    Darwin*)    MACHINE=Mac;;' >> "lib/install.sh"
    	echo '    CYGWIN*)    MACHINE=Cygwin;;' >> "lib/install.sh"
    	echo '    MINGW*)     MACHINE=MinGw;;' >> "lib/install.sh"
    	echo '    *)          MACHINE="UNKNOWN:${unameOut}"' >> "lib/install.sh"
	echo 'esac' >> "lib/install.sh"
	echo "" >> "lib/install.sh"		
	echo 'if [ -f "$MACHINE/libmelodeer.so" ]; then cp "$MACHINE/libmelodeer.so" "/usr/local/lib/"; chmod 0755 "/usr/local/lib/libmelodeer.so"; echo "Copied library to /usr/local/lib."; fi' >> "lib/install.sh"
	echo 'if [ -d /lib64 ]; then cp "$MACHINE/libmelodeer.so" "/lib64/"; chmod 0755 "/lib64/libmelodeer.so"; echo "Copied library to /lib64."; fi' >> "lib/install.sh"
	echo 'if [ "$MACHINE" = "Linux" ]; then echo "Running ldconfig."; ldconfig; fi' >> "lib/install.sh"
	echo "" >> "lib/install.sh"
	echo 'exit' >> "lib/install.sh"

	chmod +x "lib/install.sh"

fi
