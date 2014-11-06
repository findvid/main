CFLAGS = -Iinclude -Wall -std=c99 -g
LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil) -lm

BUILDFILES= obj/largelist.o obj/edgedetect.o obj/histograms.o obj/shotbounds.o

clean:
	rm -f ./test
	rm -f obj/*

obj/%.o: src/%.c
	gcc $(CFLAGS) -c -o $@ $<

obj/backend.o: $(BUILDFILES)
	ar cr $@ $(BUILDFILES)

build: $(BUILDFILES)
	gcc $(CFLAGS) $(BUILDFILES) -o shotbounds $(LIBSFFMPEG)

edgetests: obj/edgedetect.o unittests/edgetest_unit.cc
	g++ $(CFLAGS) $^ -o test $(LIBSFFMPEG) -lgtest -lpthread

histtests: src/histtests.c
	gcc $(CFLAGS) src/histtests.c -o histtests $(LIBSFFMPEG)

histograms: src/histograms.c
	gcc -c $(CFLAGS) src/histograms.c -o obj/histograms.o $(LIBSFFMPEG)

readframe: src/readframe.c
	gcc $(CFLAGS) src/readframe.c -o readframe $(LIBSFFMPEG)

histograms_test: unittests/histograms_test.c obj/histograms.o
	g++ -c $(CFLAGS) unittests/histograms_test.c -o obj/histograms_test.o
	g++ $(CFLAGS) obj/histograms_test.o obj/histograms.o -o histograms_test -lgtest -lpthread $(LIBSFFMPEG)

listtests: obj/largelist.o unittests/largelist_test.cc
	g++ $(CFLAGS) $^ -o test -lgtest -lpthread
