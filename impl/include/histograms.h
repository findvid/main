#pragma once
#include <stdint.h>
#include <libavformat/avformat.h>

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
uint32_t* newHistRgb();

/**
 * Allocates a 8x4x4 color histogram
 * The histogram is represented as a uint32 array with 128 elements.
 * Each element representing a bin.
 * It's index is organized like this: 0bHHHSSVV
 *
 * @return	pointer to the newly allocated histogram
 */
uint32_t* newHistHsv();

/**
 * Fills a color histogram with data from a frame
 * The histogram is a 4x4x4 RGB-histogram
 *
 * @hist	pointer to the histogram array
 * @img		the image a histogram should be created for. Make sure height and width are set.
 */
void fillHistRgb(uint32_t* hist, AVFrame* img);

/**
 * Fills a color histogram with data from a frame
 * The histogram is a 8x4x4 HSV-histogram
 *
 * @hist	pointer to the histogram array
 * @img		the image a histogram should be created for. Make sure height and width are set.
 */
void fillHistHsv(uint32_t* hist, AVFrame* img);

