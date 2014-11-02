#ifndef _EDGEDETECT__H_
#define _EDGEDETECT__H_

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#define getPixelG8(p,x,y) (uint8_t)((((x)>=0) && ((y)>=0) && ((y)<height) && ((x)<width))?(p->data[0][(x) + ((y) * p->linesize[0])]):127)

#define setPixelG8(p,x,y,g) p->data[0][(x) + (y) * p->linesize[0]] = (uint8_t)g

#define OPERATOR_DIRECTIONS 6

//struct to save the offsets for an operator mask to efficiently precalculate them
typedef struct {
	int width;
	short * off;
} OperatorMask;



double gauss1(double x, double deviation);

AVFrame * getEdgeProfile(AVFrame * original, struct SwsContext * swsctx, int width, int height);
#endif
