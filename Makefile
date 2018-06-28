proj=melodeer
compile=gcc main.o mdcore.o mdflac.o -l openal -l FLAC -pthread -o $(proj)

$(proj): main.o mdcore.o mdflac.o
	$(compile)

mdflac.o: mdflac.c mdflac.h
	gcc -c mdflac.c -o mdflac.o -O3

mdcore.o: mdcore.c mdcore.h
	gcc -c mdcore.c -o mdcore.o -O3

main.o: main.c
	gcc -c main.c -o main.o -O3

.PHONY=clean
clean:
	-rm main.o mdcore.o mdflac.o $(proj)

.PHONY=build
build:
	-make $(proj)

.PHONY=run
run:
	make build
	./$(proj)

.PHONY=debug
debug:
	make build
	valgrind ./$(proj)
