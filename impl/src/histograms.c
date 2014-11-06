#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <math.h>
#include "histograms.h"

#define MAX(a, b) ((a>b) ? a : b)
#define MIN(a, b) ((a<b) ? a : b)


void rgbToHsv(HSV* hsv, int r, int g, int b) {
	int min, max;
	// Get the minimum of the colors
	min = MIN(r,g);
	min = MIN(min,b);
	// Get the maximum of the colors
	max = MAX(r,g);
	max = MAX(max,b);

	// Set the value
	hsv->v = max;

	// Catch gray tones
	if (min == max) {
		hsv->h = 0;
		hsv->s = 0;
		return;
	}

	// Calculate the hue
	if (max == r) {
		hsv->h = (60 * (g - b)) / (max - min);
	} else if (max == g) {
		hsv->h = 2 * 60 + (60 * (b - r)) / (max - min);
	} else /*max == b*/ {
		hsv->h = 4 * 60 + (60 * (r - g)) / (max - min);
	}

	if (hsv->h < 0) {
		hsv->h += 360;
	}

	// Calculate the saturation
	hsv->s = (255 * (max - min)) / max;

}


uint32_t* newHistRgb() {
	return (uint32_t *)av_malloc(sizeof(uint32_t) * 64);
}

uint32_t* newHistHsv() {
	return (uint32_t *)av_malloc(sizeof(uint32_t) * 128);
}

void fillHistRgb(uint32_t* hist, AVFrame* img) {
	int i;
	// Reset the histogram to 0
	for (i = 0; i < 64; i++) {
		hist[i] = 0;
	}
	// Increase each bin for each pixel with the right color
	for (i = 0; i < img->width * img->height; i++) {
		hist[GETBINRGB(img->data[0][i * 3] , img->data[0][i * 3 + 1], img->data[0][i * 3 +2])]++;
	}
}

void fillHistHsv(uint32_t* hist, AVFrame* img) {
	int i;
	HSV hsv;
	// Reset the histogram to 0
	for (i = 0; i < 128; i++) {
		hist[i] = 0;
	}
	// Increase each bin for each pixel with the right color
	for (i = 0; i < img->width * img->height; i++) {
		// Convert to HSV
		rgbToHsv(&hsv, img->data[0][i * 3], img->data[0][i * 3 + 1], img->data[0][i * 3 + 2]);
		hist[GETBINHSV(hsv.h, hsv.s, hsv.v)]++;
	}
}

uint32_t histDiffRgb(uint32_t *h1, uint32_t *h2) {
        int i;
        int diff = 0;
	// Add up the difference per bin
        for (i = 0; i < 4*4*4; i++) {
                diff += abs(h1[i] - h2[i]);
        }
        return diff;
}

uint32_t histDiffHsv(uint32_t *h1, uint32_t *h2) {
        int i;
        int diff = 0;
	// Add up the difference per bin
        for (i = 0; i < 8*4*4; i++) {
                diff += abs(h1[i] - h2[i]);
        }
        return diff;
}
