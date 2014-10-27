#include <math.h>
#include "edgedetect.h"

AVFrame * getEdgeProfile(AVFrame *original, struct SwsContext * swsctx, int width, int height) {
	//Retrieve a grayscale copy of the image; the SwsContext must be set properly
	printf("original@0x%x\n", original);
	AVFrame *gray = av_frame_alloc();
	avpicture_alloc((AVPicture *)gray, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)gray, buffer, PIX_FMT_GRAY8, width, height);
	sws_scale(swsctx, (const uint8_t * const*)original->data, original->linesize, 0, height, gray->data, gray->linesize);


	printf("Got grayscale copy@0x%x\n", gray);
	//Improve contrast by linear scaling
	int min = 255, max = 0;

	for( int y = 0; y < height; y++ ) {
		for( int x = 0; x < width; x++ ) {
			int p = getPixelG8(gray, x, y);
			if (min > p) min = p;
			else if(max < p) max = p;
		}
	}
	printf("MIN = %i, MAX = %i\n", min, max);
	if(min!=0 || max!= 255) {
		double scaling = 255.0 / (max - min);
		for( int y = 0; y < height; y++ ) {
			for( int x = 0; x < width; x++ ) {
				gray->data[0][x + y * gray->linesize[0]] -= min;
				gray->data[0][x + y * gray->linesize[0]] *= scaling;
			}
		}
	}

	av_free(buffer);

	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);
	printf("Alloc res picture\nwidth=%i\nheight=%i\n", width, height);
	printf("linesize of grayscale-pic = %i\n", gray->linesize[0]);	
	//For each pixel, apply sodel operator to obtain grayscale intensity changes in different directions
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			printf("(%i,%i)\n", x, y);

			int dx = getPixelG8(gray,x-1,y-1) + 2 * getPixelG8(gray, x, y-1) + getPixelG8(gray, x+1, y-1) - getPixelG8(gray, x-1, y+1) - 2 * getPixelG8(gray, x, y+1) - getPixelG8(gray, x+1, y+1);
			printf("dx = %i\n", dx);
			int dy = getPixelG8(gray, x-1, y-1) + 2 * getPixelG8(gray, x-1, y) + getPixelG8(gray, x-1, y+1) - getPixelG8(gray, x+1, y-1) - 2 * getPixelG8(gray, x+1, y) - getPixelG8(gray, x+1, y+1);
			printf("dy = %i\n", dy);
			
			//each pixel saves the magnitude of the sodel operator at this point
			printf("Save [%i,%i] to 0x%x\n", dx, dy, res->data[0]+x+y*res->linesize[0]);
			res->data[0][x + y * res->linesize[0]] = (uint8_t)sqrt(dx * dx + dy * dy);
			//printf("(%i,%i)=%i\n", x, y, res->data[0][x + y * res->linesize[0]]);
		}
	}

	//Ship it
	av_free(gray);
	
	return res;
}
