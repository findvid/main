LIBSFFMPEG=$(shell pkg-config --libs libavcodec) $(shell pkg-config --libs libavformat) $(shell pkg-config --libs libswscale) $(shell pkg-config --libs libavutil)

build: frametest.c
	gcc -std=c99 frametest.c -o frametest -lavcodec -lavformat -lswscale -lavutil

build2: frametest.c
	gcc -std=c99 frametest.c -o frametest $(LIBSFFMPEG)
