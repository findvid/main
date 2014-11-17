#pragma once
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#define setPixel(p,x,y,c) \
	do { \
	p->data[0][(x) * 3 + (y)*p->linesize[0]] = ((uint8_t)(c>>16)); \
	p->data[0][(x) * 3 + (y)*p->linesize[0] + 1] = ((uint8_t)(c>>8)); \
	p->data[0][(x) * 3 + (y)*p->linesize[0] + 2] = (uint8_t)c; \
	} while(0)

#define getPixel(p,x,y) ((p->data[0][(x) * 3 + (y)*p->linesize[0]]<<16) + (p->data[0][(x) * 3 + (y)*p->linesize[0] + 1]<<8) + p->data[0][(x) * 3 + (y)*p->linesize[0] + 2])


AVFrame * copyFrame(AVFrame *pPic, struct SwsContext * ctx, int width, int height);

void SaveFrameRGB24(AVFrame *pFrame, int width, int height, int i);

void SaveFrameG8(AVFrame * pFrame, int width, int height, int i);
