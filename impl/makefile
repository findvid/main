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

histograms: src/histograms.c
	gcc -c $(CFLAGS) src/histograms.c -o obj/histograms.o $(LIBSFFMPEG)

histograms_test: unittests/histograms_test.c obj/histograms.o
	g++ -c $(CFLAGS) unittests/histograms_test.c -o obj/histograms_test.o
	g++ $(CFLAGS) obj/histograms_test.o obj/histograms.o -o histograms_test -lgtest -lpthread $(LIBSFFMPEG)
