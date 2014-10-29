#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <math.h>
#include "histograms.h"

typedef struct {
	int h;
	int s;
	int v;
} HSV;

#define MAX(a, b) ((a>b) ? a : b)
#define MIN(a, b) ((a<b) ? a : b)

void rgbToHsv(HSV* hsv, int r, int g, int b) {
	int min, max;
	min = MIN(r,g);
	min = MIN(min,b);
	max = MAX(r,g);
	max = MAX(max,b);

	if (min == max) {
		hsv->h = 0;
		hsv->s = 0;
		hsv->v = min;
		return;
	}

	if (max == r) {
		hsv->h = 60 * ((g - b) / (max - min));
	} else if (max == g) {
		hsv->h = 60 * (2 + (g - b) / (max - min));
	} else /*max == b*/ {
		hsv->h = 60 * (4 + (g - b) / (max - min));
	}

	if (hsv->h < 0) {
		hsv->h += 360;
	}

	hsv->s = (255 * (max - min)) / max;

	hsv->v = max;
}


uint32_t* newHistRgb() {
	return (uint32_t *)av_malloc(sizeof(uint32_t) * 64);
}

uint32_t* newHistHsv() {
	return (uint32_t *)av_malloc(sizeof(uint32_t) * 128);
}

void fillHistRgb(uint32_t* hist, AVFrame* img, int w, int h) {
	int i;
	// Reset the histogram to 0
	for (i = 0; i < 64; i++) {
		hist[i] = 0;
	}
	// Increase each bin for each pixel with the right color
	for (i = 0; i < w * h; i++) {
		hist[GETBINRGB(img->data[0], i * 3)]++;
	}
}

void fillHistHsv(uint32_t* hist, AVFrame* img, int w, int h) {
	int i;
	HSV hsv;
	// Reset the histogram to 0
	for (i = 0; i < 64; i++) {
		hist[i] = 0;
	}
	// Increase each bin for each pixel with the right color
	for (i = 0; i < w * h; i++) {
		// Convert to HSV
		rgbToHsv(&hsv, img->data[0][i * 3], img->data[0][i * 3 + 1], img->data[0][i * 3 + 2]);
		hist[GETBINHSV(hsv)]++;
	}
}
