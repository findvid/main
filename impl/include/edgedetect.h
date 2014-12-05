#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "fvutils.h"
#include "largelist.h"

#define getPixelG8(p,x,y) (uint8_t)((((x)>=0) && ((y)>=0) && ((y)<((p)->height)) && ((x)<((p)->width)))?((p)->data[0][(x) + ((y) * (p)->linesize[0])]):0)
//#define getPixelG8(p,x,y) (uint8_t)((p)->data[0][(x < 0?0:(x >= (p)->width?(p)->width:x)) + (y < 0?0:(y >= (p)->height?(p)->height:y)) * (p)->linesize[0]])
//#define setPixelG8(p,x,y,g) (p)->data[0][(x) + (y) * (p)->linesize[0]] = (uint8_t)(g)
#define setPixelG8(p,x,y,g) do { \
	if (x >= 0 && x < (p)->width && y >= 0 && y < (p)->height) \
		(p)->data[0][(x) + (y) * (p)->linesize[0]] = (uint8_t)g; \
	} while(0)

//Defines how many of the last elements of difference values during shot detection are returned and, in return, put back into the next call
#define MAX_FEEDBACK_LENGTH 25

#define OPERATOR_DIRECTIONS 6

#define HYSTERESIS_T1 20
#define HYSTERESIS_T2 10

//
#define QUADRANTS_WIDTH 8
#define QUADRANTS_HEIGHT 5
#define FEATURE_LENGTH (QUADRANTS_WIDTH*QUADRANTS_HEIGHT)

//#define getEdgeProfile(i,s,w,h) getEdgeProfileSodel(i,s,w,h)

//struct to save the offsets for an operator mask to efficiently precalculate them
typedef struct {
	int width;
	short * off;
	double * weights;
} OperatorMask;

struct t_sobelOutput {
	AVFrame * mag;
	AVFrame * dir;
};


/* Mask for billinear interpolation
 * stores masks for up to 9 different quadrants of width*height pixels
 * the weight at the position m(x, y) determines how much of the pixels value is used for the correspondending mask
 */
typedef struct t_interpolationWeights {
	uint32_t width;
	uint32_t height;
	double * c;
	double * nw;
	double * n;
	double * ne;
	double * e;
	double * se;
	double * s;
	double * sw;
	double * w;
} InterpolationWeights;

//Makros to get/set a matrix' vars, given the matrix pointer, row, column and linesize
#define getMatrixVar(p, x, y, l) ((p)[(x) + (y) * (l)])
#define setMatrixVar(p, v, x, y, l) (p)[(x) + (y) * (l)] = v

//Create a billinear interpolation mask for quadrants of the given dimensions
InterpolationWeights * getLinearInterpolationWeights(int width, int height);

double cmpProfiles(AVFrame * p1, AVFrame * p2);

void linearScale(AVFrame * pic);

AVFrame * getEdgeProfile(AVFrame * original, struct SwsContext * swsctx, int width, int height);
AVFrame * getEdgeProfile2(AVFrame * original, struct SwsContext * swsctx, int width, int height);

void getSobelOutput(AVFrame * frame, struct t_sobelOutput * out);

void detectCutsByEdges(LargeList * list_frames, LargeList * list_cuts, uint32_t startframe, ShotFeedback * feedback, struct SwsContext * swsctx, int width, int height);

void edgeFeatures_length(uint32_t *);
void edgeFeatures(AVFrame *, uint32_t **, InterpolationWeights *);
