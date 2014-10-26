#pragma once
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#define getPixelG8(p,x,y) ((x>=0) && (y>=0) && (y<height) && (x<width)?(p->data[0][x + y * p->linesize[0]]):0)

AVFrame * getEdgeProfile(AVFrame * original, struct SwsContext * swsctx, int width, int height);

