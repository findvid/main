#include <math.h>
#include "edgedetect.h"

void imageAdd(AVFrame * img1, AVFrame * img2, int width, int height) {
	for (int x = 0; x < width; x++ ) {
		for ( int y = 0; y < height; y++ ) {
			setPixelG8(img1, x, y, (getPixelG8(img1, x, y) + getPixelG8(img2, x, y)));
		}
	}
}

//Improve contrast by linear scaling
void linearScale(AVFrame * pic, int width, int height) {
	int min = 255, max = 0;


	for( int y = 0; y < height; y++ ) {
		for( int x = 0; x < width; x++ ) {
			int p = getPixelG8(pic, x, y);
			if (min > p) min = p;
			else if(max < p) max = p;
		}
	}

	if(min!=0 || max!= 255) {
		double scaling = 255.0 / (max - min);
		for( int y = 0; y < height; y++ ) {
			for( int x = 0; x < width; x++ ) {
				pic->data[0][x + y * pic->linesize[0]] -= min;
				pic->data[0][x + y * pic->linesize[0]] *= scaling;
			}
		}
	}
}

double gauss1(double x, double deviation) {
	return -x/(deviation*deviation) * exp(-(x*x/(2 * deviation * deviation)));
}

double gauss2(double x, double deviation) {
	return x*x/(deviation*deviation*deviation*deviation) * exp(-(x*x/(2 * deviation * deviation)));
}
/**
 * Convolve the area around a given position with a gaussian first derivative operator
 */
double convolveAt(AVFrame * pic, int x, int y, OperatorMask * mask, int width, int height) {
	double output = 0.0;

	for (int p = 0; p < mask->width; p++) {
		//G(-x)*f(x)
		output += getPixelG8(pic, x - mask->off[2 * p], y - mask->off[2 * p + 1]) * gauss1((p+1), mask->width * 3);
		//G(x)*f(-x)
		output += getPixelG8(pic, x + mask->off[2 * p], y + mask->off[2 * p + 1]) * gauss1(-(p+1), mask->width * 3);
	}

	return fabs(output);
}

AVFrame * getGaussianGradient(AVFrame * gray, OperatorMask * mask, int width, int height) {
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);

	for (int x = 0; x < width; x++)	 {
		for (int y = 0; y < height; y++) {
			setPixelG8(res, x, y, convolveAt(gray, x, y, mask, width, height));
		}
	}

	return res;
}

/**
 * Convolve image with sodel operator
 */
AVFrame * getGradientMagnitudeMap(AVFrame * gray, struct SwsContext * swsctx, int width, int height) {
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);


//For each pixel, apply sodel operator to obtain grayscale intensity changes in different directions
	double maxmag = 0.0;
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			
			//Gradients of sodel operator
			int dx = getPixelG8(gray, x-1, y-1) + 2 * getPixelG8(gray, x-1, y) + getPixelG8(gray, x-1, y+1) - getPixelG8(gray, x+1, y-1) - 2 * getPixelG8(gray, x+1, y) - getPixelG8(gray, x+1, y+1);
			int dy = getPixelG8(gray,x-1,y-1) + 2 * getPixelG8(gray, x, y-1) + getPixelG8(gray, x+1, y-1) - getPixelG8(gray, x-1, y+1) - 2 * getPixelG8(gray, x, y+1) - getPixelG8(gray, x+1, y+1);

			//normalize output
			dx /= 9;
			dy /= 9;

			//each pixel saves the magnitude of the sodel operator at this point
 			double mag = sqrt(dx * dx + dy * dy);
			if (maxmag < mag) maxmag = mag;
			res->data[0][x + y * res->linesize[0]] = (uint8_t)mag;
		}
	}

	return res;	
}


/**
 * Takes an array of masks and returns 1 if any mask matches the coord (x,y) as local maximum
 * This assumes there are as many masks as OPERATOR_DIRECTIONS defines
 */
int isEdgePixel(AVFrame * varmap, int x, int y, OperatorMask * masks, int width, int height) {
	int isEdge = 0;
	for ( int i = 0; i < OPERATOR_DIRECTIONS; i++) {
		int isLocalMax = 1;
		int center = getPixelG8(varmap, x, y);
		for (int p = 0; p < masks[i].width; p++) {
			int xo = masks[i].off[2 * p];
			int yo = masks[i].off[2 * p + 1];
			if (getPixelG8(varmap, x-xo, y-yo) >= center) {
				isLocalMax = 0;
			} else if (getPixelG8(varmap, x+xo, y+yo) >= center) {
				isLocalMax = 0;
			}
			if (isLocalMax) isEdge = 1;
		}
	}
	return isEdge;
}

AVFrame * getEdgeProfile(AVFrame *original, struct SwsContext * swsctx, int width, int height) {
	//Retrieve a grayscale copy of the image; the SwsContext must be set properly
	AVFrame *gray = av_frame_alloc();
	avpicture_alloc((AVPicture *)gray, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)gray, buffer, PIX_FMT_GRAY8, width, height);
	sws_scale(swsctx, (const uint8_t * const*)original->data, original->linesize, 0, height, gray->data, gray->linesize);

	linearScale(gray, width, height);
	
	//Initialze an empty frame
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);

	AVFrame * sodel = getGradientMagnitudeMap(gray, swsctx, width, height);
	linearScale(sodel, width, height);
	SaveFrameG8(sodel, width, height, 2);


	//Calculate some operator masks
	OperatorMask masks[OPERATOR_DIRECTIONS];
	for ( int a = 0; a < OPERATOR_DIRECTIONS; a++ ) {
		double angle = (3.1415 / OPERATOR_DIRECTIONS) * a;
		masks[a].width = 3;
		masks[a].off = malloc(sizeof(short) * 2 * masks[a].width);
		for (int p = 1; p <= masks[a].width; p++) {
			masks[a].off[2 * (p-1)] = (short)round(p * cos(angle));
			masks[a].off[2 * (p-1) + 1] = (short)round(p * sin(angle));
		}
		printf("mask offsets = [%i, %i | %i, %i]\n", masks[a].off[0], masks[a].off[1], masks[a].off[2], masks[a].off[3]);
	}
	

	//Other shit
	AVFrame * ggrad1 = getGaussianGradient(gray, &masks[0], width, height);
	SaveFrameG8(ggrad1, width, height, 11);
	AVFrame * ggrad2 = getGaussianGradient(gray, &masks[1], width, height);
	SaveFrameG8(ggrad2, width, height, 12);
	AVFrame * ggrad3 = getGaussianGradient(gray, &masks[2], width, height);
	SaveFrameG8(ggrad3, width, height, 13);
	AVFrame * ggrad4 = getGaussianGradient(gray, &masks[3], width, height);
	SaveFrameG8(ggrad4, width, height, 14);
	AVFrame * ggrad5 = getGaussianGradient(gray, &masks[4], width, height);
	SaveFrameG8(ggrad5, width, height, 15);
	AVFrame * ggrad6 = getGaussianGradient(gray, &masks[5], width, height);
	SaveFrameG8(ggrad6, width, height, 16);

	imageAdd(ggrad1, ggrad2, width, height);
	imageAdd(ggrad1, ggrad3, width, height);
	imageAdd(ggrad1, ggrad4, width, height);
	imageAdd(ggrad1, ggrad5, width, height);
	imageAdd(ggrad1, ggrad6, width, height);

	linearScale(ggrad1, width, height);
	SaveFrameG8(ggrad1, width, height, 20);
	

	av_free(ggrad2);
	av_free(ggrad3);
	av_free(ggrad4);
	av_free(ggrad5);
	av_free(ggrad6);


	//Calculate average value for intensity in ggrad1
	double avg = 0.0;
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			avg += (double)(getPixelG8(ggrad1, x, y));
		}
	}
	avg /= (width * height);
	//Mark them as pixel if any mask detects a local extremum in change
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			if (getPixelG8(ggrad1, x, y) > avg)
				res->data[0][x + y * res->linesize[0]] = 255;	
		}
	}

	SaveFrameG8(gray, width, height, 1);
	//Clean up + Ship it
	
	//wreck calculated masks
	for ( int i = 0; i < OPERATOR_DIRECTIONS; i++)
		free(masks[i].off);

	av_free(sodel);
	av_free(ggrad1);
	av_free(gray);	
	
	return res;
}
