CFLAGS = -Iinclude -Wall -std=c99 -g
LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil) -lm

BUILDFILE= obj/edgedetect.o 

obj/%.o: src/%.c
	gcc $(CFLAGS) -c -o $@ $< $(LIBSFFMPEG)

build: $(BUILDFILE) src/frametest.c
	gcc $(CFLAGS) $(BUILDFILE) src/frametest.c -o edges $(LIBSFFMPEG)

test: $(BUILDFILE) src/frametest.c 
	gcc $(CFLAGS) $(BUILDFILE) src/frametest.c -o test $(LIBSFFMPEG)

testcase1: $(BUILDFILE) src/edgetest_unit.cc
	g++ $(CFLAGS) $^ -o test $(LIBSFFMPEG) -lgtest -lpthread
