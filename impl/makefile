LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil)

BUILDFILE=

build: $(BUILDFILE)
	gcc -std=c99 -Iinclude $(BUILDFILE) -o toast $(LIBSFFMPEG)
