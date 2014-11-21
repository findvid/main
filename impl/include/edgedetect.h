#ifndef _EDGEDETECT__H_
#define _EDGEDETECT__H_

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "largelist.h"

#define setPixelG8(p,x,y,g) (p)->data[0][(x) + (y) * (p)->linesize[0]] = (uint8_t)(g)

//Defines how many of the last elements of difference values during shot detection are returned and, in return, put back into the next call
#define MAX_FEEDBACK_LENGTH 25

#define OPERATOR_DIRECTIONS 6

//#define getEdgeProfile(i,s,w,h) getEdgeProfileSodel(i,s,w,h)

//struct to save the offsets for an operator mask to efficiently precalculate them
typedef struct {
	int width;
	short * off;
	double * weights;
} OperatorMask;


//Expose these functions for testing purposes
void linearScale(AVFrame * pic, int width, int height);

//AVFrame * getEdgeProfileGauss(AVFrame * original, struct SwsContext * swsctx, int width, int height);
AVFrame * getEdgeProfile(AVFrame * original, struct SwsContext * swsctx, int width, int height);

void detectCutsByEdges(LargeList * list_frames, LargeList * list_cuts, uint32_t startframe, ShotFeedback * feedback, struct SwsContext * swsctx, int width, int height);
#endif
