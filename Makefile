proj = melodeer
objects = main.o mdcore.o mdflac.o mdwav.o mdlame.o
libs = openal FLAC pthread mp3lame

srcdir = source
builddir = build
depsdir = include

$(proj): $(objects)
	gcc $(addprefix $(builddir)/,$^) $(addprefix -l,$(libs)) -o $(proj)

%.o : $(srcdir)/%.c $(depsdir)/%.h
	gcc -c $< -o $(addprefix $(builddir)/,$@) -I$(depsdir)

main.o: $(srcdir)/main.c
	-mkdir $(builddir)
	gcc -c $< -o $(addprefix $(builddir)/,$@) -I$(depsdir)

.PHONY=clean
clean:
	-rm $(builddir)/* $(proj)

.PHONY=run
run:
	make $(proj)
	./$(proj)

.PHONY=debug
debug:
	make $(proj)
	valgrind ./$(proj)
