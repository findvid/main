#include "edgedetect.h"

AVFrame * getEdgeProfile(AVFrame *original, struct SwsContext * swsctx, int width, int height) {
	//Retrieve a grayscale copy of the image; the SwsContext must be set properly
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);
	sws_scale(swsctx, (const uint8_t * const*)original->data, original->linesize, 0, height, res->data, res->linesize);

	//Improve contrast by linear scaling
	int min = 255, max = 0;

	for( int y = 0; y < height; y++ ) {
		for( int x = 0; x < width; x++ ) {
			int p = getPixelG8(res, x, y);
			if (min > p) min = p;
			else if(max < p) max = p;
		}
	}

	double scaling = 255.0 / (max - min);
	for( int y = 0; y < height; y++ ) {
		for( int x = 0; x < width; x++ ) {
			res->data[0][x + y * res->linesize[0]] -= min;
			res->data[0][x + y * res->linesize[0]] *= scaling;
		}
	}





	//Ship it
	return res;
}
