#pragma once
#include <stdint.h>
#include <libavformat/avformat.h>
#include "largelist.h"

typedef struct {
	int h;  // 0..359
	int s;  // 0..255
	int v;  // 0..255
} HSV;

/**
 * Converts rgb colors to hsv
 *
 * @param hsv   pointer to the hsv struct
 * @param r     red 0..255
 * @param g     green 0..255
 * @param b     blue 0..255
 */
void rgbToHsv(HSV* hsv, int r, int g, int b);


#define HIST_RGB_SIZE 64
#define HIST_HSV_SIZE 128

/*
 * Returns the inxed of the bin for an rgb-histogram
 */
#define GETBINRGB(r, g, b) ( (((r) >> 6) << 4) + (((g) >> 6) << 2) + ((b) >> 6) )

/*
 * Returns the index of the bin for an hsv-histogram
 */
#define GETBINHSV(h, s, v) ( (((h) / 45) << 4) + (((s) >> 6) << 2) + ((v) >> 6) )

/**
 * Allocates a 4x4x4 color histogram
 * The histogram is represented as a uint32 array with 64 elements.
 * Each element representing a bin.
 * It's index is organized like this: 0bRRGGBB
 *
 * @return	pointer to the newly allocated histogram
 */
uint32_t* newHistRgb(void);

/**
 * Allocates a 8x4x4 color histogram
 * The histogram is represented as a uint32 array with 128 elements.
 * Each element representing a bin.
 * It's index is organized like this: 0bHHHSSVV
 *
 * @return	pointer to the newly allocated histogram
 */
uint32_t* newHistHsv(void);

/**
 * Fills a color histogram with data from a frame
 * The histogram is a 4x4x4 RGB-histogram
 *
 * @param hist	pointer to the histogram array
 * @param img	the image a histogram should be created for. Make sure height and width are set.
 */
void fillHistRgb(uint32_t* hist, AVFrame* img);

/**
 * Fills a color histogram with data from a frame
 * The histogram is a 8x4x4 HSV-histogram
 *
 * @param hist	pointer to the histogram array
 * @param img	the image a histogram should be created for. Make sure height and width are set.
 */
void fillHistHsv(uint32_t* hist, AVFrame* img);

/**
 * Calculates the difference between two 4x4x4 histograms
 * 
 * @param h1	First histogram
 * @param h2	Second histogram
 * @return	sum of the bin differences between h1 and h2
 */
uint32_t histDiffRgb(uint32_t *h1, uint32_t *h2);

/**
 * Calculates the difference between two 8x4x4 histograms
 * 
 * @param h1	First histogram
 * @param h2	Second histogram
 * @return	sum of the bin differences between h1 and h2
 */
uint32_t histDiffHsv(uint32_t *h1, uint32_t *h2);


typedef struct {
	uint32_t *last_hist;
	uint32_t last_diff;
	int last_derivation;
} ColorHistFeedback;

/**
 * Detect hard cuts useing colorhistograms
 *
 * @param list_frames	List of frames to detect cuts in.
 * @param list_cuts	List the detected cuts will be added to
 * @param frame_no	Index of the first frame. Used to add the detected cuts to the list
 * @param feedback	Feedback from the last call
 * @param histDiff	List to witch the differences of the histograms from frame to frame will get stored in
 */
void detectCutsByHistogram(LargeList *list_frames, LargeList *list_cuts, uint32_t frame_no, ColorHistFeedback *feedback, LargeList *histDiff);

uint32_t *applyRectWindow(uint32_t *signal, int len, int win);
