CFLAGS = -I=usr/include/ffmpeg -Iinclude -Wall -std=c99 -g
PYFLAGS = -pthread -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -fPIC
PYFLAGSLIB = -pthread -shared -Wl,-O1 -Wl,-Bsymbolic-functions -Wl,-Bsymbolic-functions -Wl,-z,relro -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -D_FORTIFY_SOURCE=2 -g -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security -Wl,--export-dynamic
LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil) -lm

BUILDFILES= obj/largelist.o obj/fvutils.o obj/histograms.o obj/shotbounds.o obj/edgedetect.o obj/feature_extraction.o
FEATURES = obj/fvutils.o obj/feature_extraction.o obj/histograms.o obj/largelist.o obj/edgedetect.o
UTILS = obj/largelist.o obj/fvutils.o

obj/%.o: src/%.c
	mkdir -p obj/
	gcc $(CFLAGS) $(PYFLAGS) -c -o $@ $< $(LIBSFFMPEG)

build: $(BUILDFILES)
	gcc $(CFLAGS) $(BUILDFILES) -o shotbounds $(LIBSFFMPEG)

features: $(FEATURES)
	gcc $(CFLAGS) $^ -o feats $(LIBSFFMPEG)

feature_test: $(FEATURES) unittests/feature_test.cc
	g++ $(CFLAGS) $^ -o feats $(LIBSFFMPEG) -lgtest -lpthread

pylib: $(BUILDFILES) src/findvidWrapper.c
	gcc $(CFLAGS) $(PYFLAGS) $(PYFLAGSLIB) -I/usr/include/python2.7 -D PYVERSION=2 $^ -o FindVid.so $(LIBSFFMPEG)
	-mv FindVid.so ../frontend/src/

#Use this target to compile against Python 3.4; 
pylib2: $(BUILDFILES) src/findvidWrapper.c
	gcc $(CFLAGS) $(PYFLAGS) $(PYFLAGSLIB) -I/usr/include/python3.4m -D PYVERSION=3 $^ -o FindVid.so $(LIBSFFMPEG)

frametest: src/frametest.c $(BUILDFILES)
	gcc $(CFLAGS) $^ -o frametest $(LIBSFFMPEG)

iter_test: src/frametest_iter.c obj/fvutils.o
	gcc $(CFLAGS) src/frametest_iter.c obj/fvutils.o -o itertest $(LIBSFFMPEG)

edge_cmp: src/edgecmp.c $(BUILDFILES)
	gcc $(CFLAGS) $^ -o edgecmp $(LIBSFFMPEG)

edgetests: obj/edgedetect.o unittests/edgetest_unit.cc $(UTILS)
	g++ $(CFLAGS) $^ -o test $(LIBSFFMPEG) -lgtest -lpthread

edges: $(BUILDFILES) src/frametest.c
	gcc $(CFLAGS) $(BUILDFILES) src/frametest.c -o edges $(LIBSFFMPEG)

histtests: src/histtests.c
	gcc $(CFLAGS) src/histtests.c -o histtests $(LIBSFFMPEG)

histograms: src/histograms.c
	gcc -c $(CFLAGS) src/histograms.c -o obj/histograms.o $(LIBSFFMPEG)

readframe: src/readframe.c
	gcc $(CFLAGS) src/readframe.c -o readframe $(LIBSFFMPEG)

histograms_test: unittests/histograms_test.cc obj/histograms.o
	g++ -c $(CFLAGS) unittests/histograms_test.cc -o obj/histograms_test.o
	g++ $(CFLAGS) obj/histograms_test.o obj/histograms.o -o histograms_test -lgtest -lpthread $(LIBSFFMPEG)

shotbounds: $(BUILDFILES)
	gcc $(CFLAGS) $(BUILDFILES) -o shotbounds $(LIBSFFMPEG)
	

shotbounds_test: unittests/shotbounds_test.cc $(BUILDFILES)
	g++ -c $(CFLAGS) unittests/shotbounds_test.cc -o obj/shotbounds_test.o
	g++ $(CFLAGS) obj/shotbounds_test.o $(BUILDFILES) -o shotbounds_test -lgtest -lpthread $(LIBSFFMPEG)

listtests: obj/largelist.o unittests/largelist_test.cc
	g++ $(CFLAGS) $^ -o test -lgtest -lpthread

fvtests: obj/fvutils.o unittests/fvtest.cc 
	g++ $(CFLAGS) $^ -o test -lgtest -lpthread $(LIBSFFMPEG)

framerate_denethor: obj/fvutils.o unittests/fps_denethor.c
	gcc $(CFLAGS) $^ -o test $(LIBSFFMPEG)

clean:
	rm -f ./test
	rm -rf obj/
	rm -f shotbounds


thumbs: obj/thumbnails2.o
	gcc $(CFLAGS) obj/thumbnails2.o -o thumbnails $(LIBSFFMPEG)
