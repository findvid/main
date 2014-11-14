CFLAGS = -Iinclude -Wall -std=c99 -g
LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil) -lm

BUILDFILES= obj/largelist.o obj/fvutils.o obj/histograms.o obj/shotbounds.o # obj/edgedetect.o 

obj/%.o: src/%.c
	mkdir -p obj/
	gcc $(CFLAGS) -c -o $@ $< $(LIBSFFMPEG)

build: $(BUILDFILES)
	gcc $(CFLAGS) $(BUILDFILES) -o shotbounds $(LIBSFFMPEG)

frametest: src/frametest.c $(BUILDFILES)
	gcc $(CFLAGS) obj/fvutils.o obj/edgedetect.o obj/largelist.o src/frametest.c -o frametest $(LIBSFFMPEG)

edgetests: obj/edgedetect.o unittests/edgetest_unit.cc
	g++ $(CFLAGS) $^ -o test $(LIBSFFMPEG) -lgtest -lpthread

histtests: src/histtests.c
	gcc $(CFLAGS) src/histtests.c -o histtests $(LIBSFFMPEG)

histograms: src/histograms.c
	gcc -c $(CFLAGS) src/histograms.c -o obj/histograms.o $(LIBSFFMPEG)

readframe: src/readframe.c
	gcc $(CFLAGS) src/readframe.c -o readframe $(LIBSFFMPEG)

histograms_test: unittests/histograms_test.cc obj/histograms.o
	g++ -c $(CFLAGS) unittests/histograms_test.cc -o obj/histograms_test.o
	g++ $(CFLAGS) obj/histograms_test.o obj/histograms.o -o histograms_test -lgtest -lpthread $(LIBSFFMPEG)

shotbounds_test: unittests/shotbounds_test.cc obj/shotbounds.o obj/histograms.o obj/edgedetect.o obj/largelist.o
	g++ -c $(CFLAGS) unittests/shotbounds_test.cc -o obj/shotbounds_test.o
	g++ $(CFLAGS) obj/shotbounds_test.o obj/histograms.o obj/shotbounds.o obj/edgedetect.o obj/largelist.o -o shotbounds_test -lgtest -lpthread $(LIBSFFMPEG)

listtests: obj/largelist.o unittests/largelist_test.cc
	g++ $(CFLAGS) $^ -o test -lgtest -lpthread

clean:
	rm -f ./test
	rm -rf obj/
	rm -f shotbounds

