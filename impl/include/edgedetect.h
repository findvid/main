#ifndef _EDGEDETECT__H_
#define _EDGEDETECT__H_

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#define getPixelG8(p,x,y) (uint8_t)((((x)>=0) && ((y)>=0) && ((y)<height) && ((x)<width))?((p)->data[0][(x) + ((y) * (p)->linesize[0])]):0)

#define setPixelG8(p,x,y,g) (p)->data[0][(x) + (y) * (p)->linesize[0]] = (uint8_t)(g)

#define OPERATOR_DIRECTIONS 6

#define getEdgeProfile(i,s,w,h) getEdgeProfileSodel(i,s,w,h)

//struct to save the offsets for an operator mask to efficiently precalculate them
typedef struct {
	int width;
	short * off;
	double * weights;
} OperatorMask;


//Expose these functions for testing purposes
void imageAdd(AVFrame * img1, AVFrame **imgs, int c, int width, int height);
void linearScale(AVFrame * pic, int width, int height);
double gauss1(double x, double deviation);
double gauss2(double x, double deviation);

AVFrame * getGaussianGradient(AVFrame *, OperatorMask *, int width, int height);

AVFrame * getEdgeProfileGauss(AVFrame * original, struct SwsContext * swsctx, int width, int height);
AVFrame * getEdgeProfileSodel(AVFrame * original, struct SwsContext * swsctx, int width, int height);
#endif
