#pragma once

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

//Include this for other code sections using FFMPEG
#include <libavutil/mem.h>

#define setPixel(p,x,y,c) \
	do { \
	p->data[0][(x) * 3 + (y)*p->linesize[0]] = ((uint8_t)(c>>16)); \
	p->data[0][(x) * 3 + (y)*p->linesize[0] + 1] = ((uint8_t)(c>>8)); \
	p->data[0][(x) * 3 + (y)*p->linesize[0] + 2] = (uint8_t)c; \
	} while(0)

#define getPixel(p,x,y) ((p->data[0][(x) * 3 + (y)*p->linesize[0]]<<16) + (p->data[0][(x) * 3 + (y)*p->linesize[0] + 1]<<8) + p->data[0][(x) * 3 + (y)*p->linesize[0] + 2])


typedef struct {
	AVFrame * lastFrame;
	int diff_len; //The feedback might be shorter than the set value because the bulk ended prematurely
	double * diff;
} ShotFeedback;


AVFrame * copyFrame(AVFrame *pPic, struct SwsContext * ctx, int width, int height);
void SaveFrameRGB24(AVFrame *pFrame, int width, int height, int i);
void SaveFrameG8(AVFrame * pFrame, int width, int height, int i);
int drawGraph(uint32_t *data, int len, int height, double scale, int nr);

//Video iterator
typedef struct {
	struct AVFormatContext * fctx;
	struct AVCodecContext * cctx;
	int videoStream;
} VideoIterator;

VideoIterator * get_VideoIterator(char *);
AVFrame * nextFrame(VideoIterator *, int *);
void readFrame(VideoIterator *, AVFrame *, int *);
void destroy_VideoIterator(VideoIterator *);
