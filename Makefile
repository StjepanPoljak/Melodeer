proj = melodeer
objects = main.o mdcore.o mdflac.o mdwav.o mdlame.o
libs = openal FLAC pthread mp3lame
libdir = /usr/local/lib
lib64dir = /lib64
incdir = /usr/local/include

srcdir = source
builddir = build
depsdir = include

$(proj): $(objects)
	gcc $(addprefix $(builddir)/,$^) $(addprefix -l,$(libs)) -o $(proj)

%.o : $(srcdir)/%.c $(depsdir)/%.h
	gcc -c -fPIC $< -o $(addprefix $(builddir)/,$@) -I$(depsdir) -O3

main.o: $(srcdir)/main.c
	-mkdir $(builddir)
	gcc -c -fPIC $< -o $(addprefix $(builddir)/,$@) -I$(depsdir) -O3

.PHONY=clean
clean:
	-rm $(builddir)/* $(proj)

.PHONY=clso
clso:
	-rm $(lib64dir)/lib$(proj).so
	-rm $(libdir)/lib$(proj).so
	-rm -rf $(incdir)/$(proj)
	ldconfig

shared: mdcore.o mdflac.o mdwav.o mdlame.o
	gcc -shared $(addprefix $(builddir)/,$^) $(addprefix -l,$(libs)) -o lib$(proj).so
	-cp lib$(proj).so $(lib64dir)/
	mv lib$(proj).so $(libdir)/
	chmod 0755 $(libdir)/lib$(proj).so
	-chmod 0755 $(lib64dir)/lib$(proj).so
	-mkdir $(incdir)/$(proj)
	cp $(depsdir)/*.h $(incdir)/$(proj)/
	-ldconfig

.PHONY=run
run:
	make $(proj)
	./$(proj)

.PHONY=debug
debug:
	make $(proj)
	valgrind ./$(proj)
