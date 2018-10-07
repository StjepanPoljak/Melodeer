proj = melodeer
objects = main.o mdflac.o mdwav.o mdlame.o mdlog.o
libs = openal FLAC pthread mp3lame
libdir = /usr/local/lib
lib64dir = /lib64
incdir = /usr/local/include

srcdir = source
builddir = build
depsdir = include

$(proj): $(objects) mdcore.o
	gcc $(addprefix $(builddir)/,$^) $(addprefix -l,$(libs)) -o $(proj)
	-./save.sh

%.o : $(srcdir)/%.c $(depsdir)/%.h
	gcc -c $< -o $(addprefix $(builddir)/,$@) -I$(depsdir) -O3 $(OFLAGS)

mdcore.o : $(srcdir)/mdcore.c $(depsdir)/mdcore.h
	gcc -c $(srcdir)/mdcore.c -o $(addprefix $(builddir)/,$@) -I$(depsdir) -O3 $(MDCOREFLAGS)

main.o: $(srcdir)/main.c
	@-mkdir $(builddir)
	gcc -c $< -o $(addprefix $(builddir)/,$@) -I$(depsdir) -O3 $(MAINFLAGS)

.PHONY=debug
debug: MDCOREFLAGS=-D MDCORE_DEBUG
debug: $(proj)

.PHONY=clean
clean:
	-rm $(builddir)/* $(proj)
	-rm mdcore.log

.PHONY=uninstall
uninstall:
	-rm $(lib64dir)/lib$(proj).so
	-rm $(libdir)/lib$(proj).so
	-rm -rf $(incdir)/$(proj)
	ldconfig

.PHONY=shared_common
shared_common:
	@./sudo_check.sh "e.g. sudo make shared or sudo make shared_debug" 
	-mkdir $(builddir)

.PHONY=shared_debug
shared_debug: shared_common
shared_debug: MDCOREFLAGS=-fPIC -D MDCORE_DEBUG
shared_debug: shared_internal

.PHONY=shared
shared: shared_common
shared: MDCOREFLAGS=-fPIC
shared: shared_internal

shared_internal: OFLAGS=-fPIC
shared_internal: mdcore.o mdflac.o mdwav.o mdlame.o mdlog.o
	gcc -shared $(addprefix $(builddir)/,$^) $(addprefix -l,$(libs)) -o lib$(proj).so
	-cp lib$(proj).so $(lib64dir)/
	mv lib$(proj).so $(libdir)/
	chmod 0755 $(libdir)/lib$(proj).so
	-chmod 0755 $(lib64dir)/lib$(proj).so
	-mkdir $(incdir)/$(proj)
	cp $(depsdir)/*.h $(incdir)/$(proj)/
	-ldconfig
	@-./save.sh

.PHONY=run
run: $(proj)
	./$(proj)

.PHONY=valgrind
valgrind: $(proj)
	valgrind ./$(proj)
