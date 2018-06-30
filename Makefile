proj = melodeer
objects = main.o mdcore.o mdflac.o mdwav.o
libs = openal FLAC pthread

$(proj): $(objects)
	gcc $^ $(addprefix  -l,$(libs)) -o $(proj)

%.o : %.c %.h
	gcc -c $< -o $@ -O3

main.o: main.c
	gcc -c main.c -o main.o -O3

.PHONY=clean
clean:
	-rm main.o mdcore.o mdflac.o mdwav.o $(proj)

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
