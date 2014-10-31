#include <math.h>
#include "edgedetect.h"

double gauss1(double x, double deviation) {
	return -x/(deviation*deviation) * exp(-(x*x/(2 * deviation * deviation)));
}

//Convolve the area around a given position with a gaussian first derivative operator
double convolveAt(AVFrame * pic, int width, int height, int x, int y, int opwidth, double dir) {
	//Checking the solitary pixel naturally cannot yield any impulse response
	if (opwidth == 1) return 0.0;

	//Get the value of (x,y) to normalize surrounding points
	int center = getPixelG8(pic, x, y); //to be projected to 0

	//Offsets from center
	int x1,y1,x2,y2;
	double output = 0.0;
	for (int w = 1; w < opwidth; w++) {
		x1 = round(w * cos(dir));
		y1 = round(w * sin(dir));
		x2 = round(-w * cos(dir));
		y2 = round(-w * sin(dir));
		
		//G(x)*f(-x)
		int px = getPixelG8(pic, x1, y1);
		output += getPixelG8(pic, x1, y1) * gauss1((double)(-w), opwidth*3.0);
		output += getPixelG8(pic, x2, y2) * gauss1((double)w, opwidth*3.0);
	}

	return output;
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

	//For each pixel, apply sodel operator to obtain grayscale intensity changes in different directions
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {

			//Gradients of sodel operator
			int dx = getPixelG8(gray, x-1, y-1) + 2 * getPixelG8(gray, x-1, y) + getPixelG8(gray, x-1, y+1) - getPixelG8(gray, x+1, y-1) - 2 * getPixelG8(gray, x+1, y) - getPixelG8(gray, x+1, y+1);
			int dy = getPixelG8(gray,x-1,y-1) + 2 * getPixelG8(gray, x, y-1) + getPixelG8(gray, x+1, y-1) - getPixelG8(gray, x-1, y+1) - 2 * getPixelG8(gray, x, y+1) - getPixelG8(gray, x+1, y+1);

			//normalize output
			dx /= 9;
			dy /= 9;

			//printf some convolving vars...
			printf("conv(%i,%i) = %f\n", x, y, convolveAt(gray, width, height, x, y, 2, 0.0));

			//each pixel saves the magnitude of the sodel operator at this point
			res->data[0][x + y * res->linesize[0]] = (uint8_t)sqrt(dx * dx + dy * dy);
		}
	}

	SaveFrameG8(gray, width, height, 1);
	//Ship it
	av_free(gray);	
	return res;
}
