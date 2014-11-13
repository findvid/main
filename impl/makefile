CFLAGS = -Iinclude -Wall -std=c99 -g
LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil) -lm

BUILDFILES= obj/thumbnails.o

clean:
	rm -f ./test
	rm -f obj/*

obj/%.o: src/%.c
	gcc $(CFLAGS) -c -o $@ $< $(LIBSFFMPEG)

build: $(BUILDFILES)
	gcc $(CFLAGS) $(BUILDFILES) -o thumbnails $(LIBSFFMPEG)

build_alt: obj/thumbnails2.o
	gcc $(CFLAGS) obj/thumbnails2.o -o thumbnails $(LIBSFFMPEG)
