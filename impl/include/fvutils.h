#pragma once

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

//Include this for other code sections using FFMPEG
#include <libavutil/mem.h>

#define setPixel(p,x,y,c) \
	do { \
	p->data[0][(x) * 3 + (y)*p->linesize[0]] = ((uint8_t)(c>>16)); \
	p->data[0][(x) * 3 + (y)*p->linesize[0] + 1] = ((uint8_t)(c>>8)); \
	p->data[0][(x) * 3 + (y)*p->linesize[0] + 2] = (uint8_t)c; \
	} while(0)

#define getPixel(p,x,y) ((p->data[0][(x) * 3 + (y)*p->linesize[0]]<<16) + (p->data[0][(x) * 3 + (y)*p->linesize[0] + 1]<<8) + p->data[0][(x) * 3 + (y)*p->linesize[0] + 2])


typedef struct {
	AVFrame * lastFrame;
	int diff_len; //The feedback might be shorter than the set value because the bulk ended prematurely
	double * diff;
} ShotFeedback;


/**
 * Returns a newly allocated copy of a AVFrame scaled with SwsContext ctx
 *
 * @param pPic		AVFrame to be copyed
 * @param ctx		SwsContext for scaleing
 * @param width		Width of the frame
 * @param height	Height of the frame
 *
 * @return A newly allocated scaled copy of pPic
 */
AVFrame * copyFrame(AVFrame *pPic, struct SwsContext * ctx, int width, int height);

/**
 * Saves a RGB24 frame as ppm-file with the filename "frame<i>.ppm" 
 *
 * @param pFrame	Frame to be saved on disk
 * @param width		Width of the frame
 * @param height	Height of the frame
 * @param i		Number for the filename "frame<i>.ppm"
 */
void SaveFrameRGB24(AVFrame *pFrame, int width, int height, int i);

/**
 * Saves a G8 frame as ppm-file with the filename "frame<i>.ppm" 
 *
 * @param pFrame	Frame to be saved on disk
 * @param width		Width of the frame
 * @param height	Height of the frame
 * @param i		Number for the filename "frame<i>.ppm"
 */
void SaveFrameG8(AVFrame * pFrame, int width, int height, int i);

/**
 * Draws a graph into the file "graph<nr>.ppm"
 *
 * @param data		Array of values to draw a graph of
 * @param len		Length of the data-array
 * @param height	Height the resulting image file should have
 * @param scale		Factor at which the values of data will get scaled
 * @param nr		Number for the filename "graph<nr>.ppm"
 *
 * @return		0 on success, currently it will always be 0
 */
int drawGraph(uint32_t *data, int len, int height, double scale, int nr);

//Video iterator
typedef struct {
	struct AVFormatContext * fctx;
	struct AVCodecContext * cctx;
	int videoStream;
} VideoIterator;

/*
 * Allocates and initialises a new video iterator for
 * the video file given in filename.
 * Use destroy_VideoIterator() to close all files and free memory.
 *
 * @param filename	Path to the video file to iterate over.
 *
 * @return		Pointer to a Videointerator on success,
 *			NULL on failure
 */
VideoIterator * get_VideoIterator(const char * filename);

/**
 * Gets the next frame from an VideoIterator.
 * The AVFrame gets newly allocated and needs to be freed
 * using av_frame_free().
 *
 * @param vidIt		The video iterator to get the frame from
 * @param gotFrame	Will be set to 1 when a frame was obtained
 *			As for now it will only be 0 when NULL
 *			is returned. Can be NULL.
 *
 * @return		On success the next frame for the iterator
 *			On failure or at EOF NULL
 */
AVFrame * nextFrame(VideoIterator * vidIt, int * gotFrame);

/**
 * Reads the next frame of vidIt into targetFrame.
 *
 * @param vidIt		The video iterator to get the frame from
 * @param gotFrame	Will be set to 1 when a frame was obtained
 *			As for now it will only be 0 on error.
 *			Can be NULL.
 * 
 */
void readFrame(VideoIterator * vidIt, AVFrame * targetFrame, int * gotFrame);

/**
 * Closed open streams and frees a VideoIterator.
 *
 * @param vidIt		The VideoIterator to be destroied
 */
void destroy_VideoIterator(VideoIterator * vidIt);
