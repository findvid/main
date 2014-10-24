CFLAGS = -Iinclude -Wall -std=c99
LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil)

BUILDFILE= 

obj/%.o: src/%.c
	gcc $(CFLAGS) -c -o $@ $<

build: $(BUILDFILE)
	gcc $(CFLAGS) $(BUILDFILE) -o shotbounds $(LIBSFFMPEG)

test: src/frametest.c
	gcc $(CFLAGS) src/frametest.c -o test $(LIBSFFMPEG)

histtests: src/histtests.c
	gcc $(CFLAGS) src/histtests.c -o histtests $(LIBSFFMPEG)
