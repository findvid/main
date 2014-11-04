CFLAGS = -Iinclude -Wall -std=c99 -g
LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil) -lm

BUILDFILES= obj/edgedetect.o obj/histograms.o obj/shotbounds.o

test: src/frametest.c

obj/%.o: src/%.c
	gcc $(CFLAGS) -c -o $@ $< $(LIBSFFMPEG)

build: $(BUILDFILES)
	gcc $(CFLAGS) $(BUILDFILES) -o shotbounds $(LIBSFFMPEG)

testcase1: $(BUILDFILES) src/edgetest_unit.cc
	g++ $(CFLAGS) $^ -o test $(LIBSFFMPEG) -lgtest -lpthread

	gcc $(CFLAGS) src/frametest.c -o test $(LIBSFFMPEG)

histtests: src/histtests.c
	gcc $(CFLAGS) src/histtests.c -o histtests $(LIBSFFMPEG)

histograms: src/histograms.c
	gcc -c $(CFLAGS) src/histograms.c -o obj/histograms.o $(LIBSFFMPEG)

readframe: src/readframe.c
	gcc $(CFLAGS) src/readframe.c -o readframe $(LIBSFFMPEG)

histograms_test: unittests/histograms_test.c obj/histograms.o
	g++ -c $(CFLAGS) unittests/histograms_test.c -o obj/histograms_test.o
	g++ $(CFLAGS) obj/histograms_test.o obj/histograms.o -o histograms_test -lgtest -lpthread $(LIBSFFMPEG)
