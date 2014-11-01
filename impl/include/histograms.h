#pragma once


/**
 *
 */
#define GETBINHSV(hsv) (((hsv.h / 45) << 4) + ((hsv.s >> 6) << 2) + (hsv.v >> 6))

/**
 * 
 */
#define GETBINRGB(img, p) (((img[p] >> 6) << 4) + ((img[p+1] >> 6) << 2) + (img[p+2] >> 6))

/**
 * Allocates a 4x4x4 color histogram
 *
 * @return	pointer to the newly allocated histogram
 */
uint32_t* newHistRgb();

/**
 * Allocates a 8x4x4 color histogram
 *
 * @return	pointer to the newly allocated histogram
 */
uint32_t* newHistHsv();

/**
 * Fills a color histogram with data from a frame
 * The histogram is a 4x4x4 RGB-histogram
 *
 * @hist	pointer to the histogram array
 * @img		the image a histogram should be created for
 * @h		height of the frame
 * @w		width of the frame
 */
void fillHistRgb(uint32_t* hist, AVFrame* img, int h, int w);

/**
 * Fills a color histogram with data from a frame
 * The histogram is a 8x4x4 HSV-histogram
 *
 * @hist	pointer to the histogram array
 * @img		the image a histogram should be created for
 * @h		height of the frame
 * @w		width of the frame
 */
void fillHistHsv(uint32_t* hist, AVFrame* img, int h, int w);

