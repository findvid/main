CFLAGS = -Iinclude -Wall -std=c99 -g
LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil) -lm

BUILDFILE= obj/edgedetect.o 

obj/%.o: src/%.c
	gcc $(CFLAGS) -c -o $@ $<

build: $(BUILDFILE)
	gcc $(CFLAGS) $(BUILDFILE) -o shotbounds $(LIBSFFMPEG)

test: $(BUILDFILE) src/frametest.c 
	gcc $(CFLAGS) $(BUILDFILE) src/frametest.c -o test $(LIBSFFMPEG)

testcase1: $(BUILDFILE) src/edgetest.c 
	gcc $(CFLAGS) $(BUILDFILE) src/edgetest.c -o test $(LIBSFFMPEG)


