#include <stdint.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <math.h>
#include "histograms.h"
#include "largelist.h"

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


uint32_t* newHistRgb(void) {
	uint32_t* result = (uint32_t *)calloc(sizeof(uint32_t), HIST_RGB_SIZE);
	if (!result) {
		printf("Calloc failed in newHistRgb()\n");
		exit(1);
	}
	return result;
}

uint32_t* newHistHsv(void) {
	uint32_t* result = (uint32_t *)calloc(sizeof(uint32_t), HIST_HSV_SIZE);
	if (!result) {
		printf("Calloc failed in newHistRgb()\n");
		exit(1);
	}
	return result;
}

void fillHistRgb(uint32_t* hist, AVFrame* img) {
	int i;
	// Reset the histogram to 0
	for (i = 0; i < HIST_RGB_SIZE; i++) {
		hist[i] = 0;
	}
	// Increase each bin for each pixel with the right color
	for (i = 0; i < img->width * img->height; i++) {
		hist[GETBINRGB(img->data[0][i * 3] , img->data[0][i * 3 + 1], img->data[0][i * 3 + 2])]++;
	}
}

void fillHistHsv(uint32_t* hist, AVFrame* img) {
	int i;
	HSV hsv;
	// Reset the histogram to 0
	for (i = 0; i < HIST_HSV_SIZE; i++) {
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
        for (i = 0; i < HIST_RGB_SIZE; i++) {
                diff += abs(h1[i] - h2[i]);
        }
        return diff;
}

uint32_t histDiffHsv(uint32_t *h1, uint32_t *h2) {
        int i;
        int diff = 0;
	// Add up the difference per bin
        for (i = 0; i < HIST_HSV_SIZE; i++) {
                diff += abs(h1[i] - h2[i]);
        }
        return diff;
}

#define CUT_DETECT_LEVEL 20000 // Magic number that hopefully does the trick
void detectCutsByHistogram(LargeList *list_frames, LargeList *list_cuts, uint32_t frame_no, ColorHistFeedback *feedback, LargeList *frameDiff) {
	// get the values of the last feedback for the start
	uint32_t *h1 = feedback->last_hist;
	uint32_t *tmp; // Histogram for the last frame
	uint32_t *h2 = newHistHsv(); // Histogram for the current frame
	uint32_t d1 = feedback->last_diff; // difference from the second last to the last frame
	uint32_t d2; // difference from the last to this frame
	int dd1 = feedback->last_derivation; // derivation last
	int dd2; // derivation this

	// Get the iterator and the first frame
	ListIterator *it_frames = list_iterate(list_frames);
	AVFrame *frame = (AVFrame *)list_next(it_frames);
	while (frame != NULL) {
		// Get the histogramm for this frame
		fillHistHsv(h2, frame);
		// Get the difference to the last frame
		d2 = histDiffHsv(h1, h2);
		// Get the derivation of the difference
		dd2 = d2 - d1;

		// Detect hard cut when there was a big enough spike and it wasn't the start of the video
		if ((dd1 >= CUT_DETECT_LEVEL) && (dd2 <= (-CUT_DETECT_LEVEL)) && (frame_no > 2)) {
			// Put the frame before the cut on the list
			// TODO printf("d1: %d, d2: %d, dd1: %d, dd2: %d\n", d1, d2, dd1, dd2);
			// TODO printf("Cut at Frame %d\n", frame_no - 2);
			list_push(list_cuts, (void *)(intptr_t)(frame_no - 2));
		}

		// Push the difference between the frames on the list for later evaluation
		list_push(frameDiff, (void *)(intptr_t)d2);

		// Swap the last and this histogram
		tmp = h1;
		h1 = h2;
		h2 = tmp;

		// Set the last difference and derivation to the results from this frame
		d1 = d2;
		dd1 = dd2;

		// Get the next frame
		frame = (AVFrame *)list_next(it_frames);
		frame_no++;
	}
	// Free the not anymore needed histogram
	free(h2);
	free(it_frames);
	// Set the feedback for the next run
	feedback->last_hist = h1;
	feedback->last_diff = d1;
	feedback->last_derivation = dd1;
}

uint32_t *applyRectWindow(uint32_t *signal, int len, int win) {
	int i;
	uint32_t *retval = (uint32_t *)calloc(len, sizeof(uint32_t));
	uint64_t sum = 0;
	int winWidth = win * 2;
	for (i = 0; i < winWidth; i++) {
		sum += signal[i];
	}
	for (i = win; i < (len - win); i++) {
		retval[i] = sum / winWidth;
		sum -= signal[i - win];
		sum += signal[i + win];
	}
	return retval;
}
