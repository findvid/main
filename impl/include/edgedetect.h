#ifndef _EDGEDETECT__H_
#define _EDGEDETECT__H_

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "fvutils.h"
#include "largelist.h"

#define getPixelG8(p,x,y) (uint8_t)((((x)>=0) && ((y)>=0) && ((y)<((p)->height)) && ((x)<((p)->width)))?((p)->data[0][(x) + ((y) * (p)->linesize[0])]):0)
#define setPixelG8(p,x,y,g) (p)->data[0][(x) + (y) * (p)->linesize[0]] = (uint8_t)(g)

//Defines how many of the last elements of difference values during shot detection are returned and, in return, put back into the next call
#define MAX_FEEDBACK_LENGTH 25

#define OPERATOR_DIRECTIONS 6

#define HYSTERESIS_T1 16
#define HYSTERESIS_T2 4

//#define getEdgeProfile(i,s,w,h) getEdgeProfileSodel(i,s,w,h)

//struct to save the offsets for an operator mask to efficiently precalculate them
typedef struct {
	int width;
	short * off;
	double * weights;
} OperatorMask;


double cmpProfiles(AVFrame * p1, AVFrame * p2);

void linearScale(AVFrame * pic);

AVFrame * getEdgeProfile(AVFrame * original, struct SwsContext * swsctx);

void detectCutsByEdges(LargeList * list_frames, LargeList * list_cuts, uint32_t startframe, ShotFeedback * feedback, struct SwsContext * swsctx);
#endif
