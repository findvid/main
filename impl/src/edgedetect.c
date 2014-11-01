#include <math.h>
#include "edgedetect.h"

double gauss1(double x, double deviation) {
	return -x/(deviation*deviation) * exp(-(x*x/(2 * deviation * deviation)));
}

double gauss2(double x, double deviation) {
	return x*x/(deviation*deviation*deviation*deviation) * exp(-(x*x/(2 * deviation * deviation)));
}
//Convolve the area around a given position with a gaussian first derivative operator
double convolveAt(AVFrame * pic, int width, int height, int x, int y, int opwidth, double dir) {
	//Checking the solitary pixel naturally cannot yield any impulse response
	if (opwidth == 1) return 255.0;


	//Offsets from center
	int xo,yo;
	double output = 0.0;
	for (int w = (-opwidth + 1); w < opwidth; w++) {
		xo = round(w * cos(dir));
		yo = round(w * sin(dir));
		//G'(-x)*f(x)
		output += (getPixelG8(pic, x+xo, y+yo) - getPixelG8(pic, x+xo-1, y+yo-1)) * gauss2((double)(-w), opwidth*3.0);
	}
	return output;
}

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
			}
			if (getPixelG8(varmap, x+xo, y+yo) >= center) {
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


	//Improve contrast by linear scaling
	int min = 255, max = 0;


	for( int y = 0; y < height; y++ ) {
		for( int x = 0; x < width; x++ ) {
			int p = getPixelG8(gray, x, y);
			if (min > p) min = p;
			else if(max < p) max = p;
		}
	}

	if(min!=0 || max!= 255) {
		double scaling = 255.0 / (max - min);
		for( int y = 0; y < height; y++ ) {
			for( int x = 0; x < width; x++ ) {
				gray->data[0][x + y * gray->linesize[0]] -= min;
				gray->data[0][x + y * gray->linesize[0]] *= scaling;
			}
		}
	}


	//av_free(buffer); Don't! The pointer is contained in 'gray' and will be freed with it! Supposedly...
	
	//Initialze an empty frame
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);

	AVFrame * sodel = getGradientMagnitudeMap(gray, swsctx, width, height);
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
	


	//Apply different directional masks to each pixel
	//Mark them as pixel if any mask detects a local extremum in change
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			if (isEdgePixel(sodel, x, y, &masks, width, height))
				res->data[0][x + y * res->linesize[0]] = 255;	
		}
	}

	SaveFrameG8(gray, width, height, 1);
	//Clean up + Ship it
	
	//wreck calculated masks
	for ( int i = 0; i < OPERATOR_DIRECTIONS; i++)
		free(masks[i].off);

	av_free(sodel);
	av_free(gray);	
	
	return res;
}
