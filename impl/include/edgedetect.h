#pragma once
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#define getPixelG8(p,x,y) (uint8_t)((((x)>=0) && ((y)>=0) && ((y)<height) && ((x)<width))?(p->data[0][(x) + ((y) * p->linesize[0])]):127)

AVFrame * getEdgeProfile(AVFrame * original, struct SwsContext * swsctx, int width, int height);

