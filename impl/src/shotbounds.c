#include <unistd.h>

#include "fvutils.h"
#include "largelist.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "histograms.h"

//#define MAX_MEMORY_USAGE ((uint32_t)(3 * 1024 * 1024 * 1024))
//always makes signed ints...not good
//uint32_t MAX_MEMORY_USAGE = 3221225472 ;//3 * 1024 * 1024 * 1024;
uint32_t MAX_MEMORY_USAGE = 512 * 1024 * 1024; // For small testing systems...like a little useless black laptop


#define DESTINATION_WIDTH 320
#define DESTINATION_HEIGHT 200

#define TOTAL_FRAMES_IN_MEMORY (MAX_MEMORY_USAGE / (DESTINATION_WIDTH * DESTINATION_HEIGHT * 3))


uint32_t mergeListsToArray(LargeList *l1, LargeList *l2, uint32_t **array) {
	// Get the maximum size the array will have
	uint32_t size = l1->size + l2->size;
	// Allocate the array
	*array = calloc(sizeof(uint32_t), size);

	uint32_t i = 0;
	void *v_e;
	void *v_c;
	// Get the iterators
	ListIterator *iter_e = list_iterate(l1);
	ListIterator *iter_c = list_iterate(l2);
	v_e = list_next(iter_e);
	v_c = list_next(iter_c);
	// while both lists contain something...
	while ((v_e != NULL) && (v_c != NULL)) {
		uint32_t e = (uint32_t)(intptr_t)v_e;
		uint32_t c = (uint32_t)(intptr_t)v_c;
		// ...merge their values.
		if ( e == c ) {
			(*array)[i++] = c;
			v_e = list_next(iter_e);
			v_c = list_next(iter_c);
		} else if (e < c) {
			(*array)[i++] = e;
			v_e = list_next(iter_e);
		} else {
			(*array)[i++] = c;
			v_c = list_next(iter_c);
		}
	}
	// Write the left over list into the array too
	while(v_e != NULL) {
		(*array)[i++] = (uint32_t)(intptr_t)v_e;
		v_e = list_next(iter_e);
	}
	while(v_c != NULL) {
		(*array)[i++] = (uint32_t)(intptr_t)v_c;
		v_c = list_next(iter_c);
	}
	free(iter_e);
	free(iter_c);
	// Return how many different values were actually written into the array
	return i;
}


int processVideo(char *filename, uint32_t **cuts) {
	av_register_all();
	// Count of frames in the video
	int frameCount = 0;
	// Frame number at which the current bulk starts
	int bulkStart = 0;
	AVFrame *pFrame = NULL;

	// Open the videofile
	VideoIterator *vidIt = get_VideoIterator(filename);
	if (!vidIt) {
		fprintf(stderr, "Could not read video file!\n");
		return -1;
	}

	// Object needed to perform conversions from a source dimension to a destination dimension using certain filters
	// Convert into a smaller frame for easier processing
	struct SwsContext *convert_rgb24 = sws_getContext(vidIt->cctx->width, vidIt->cctx->height, vidIt->cctx->pix_fmt, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	// Feedback struct for detectCutsByColors()
	ColorHistFeedback feedback_colors;
	feedback_colors.last_hist = newHistHsv();
	feedback_colors.last_diff = 0;
	feedback_colors.last_derivation = 0;

	// List to contain all the small frames and pass it to processing when the video is finished or one bulk is filled (defined by MAX_MEMORY_USAGE)
	// Each block of data should be the size of a mempage and contains pointers.
	// Subtract this value so the list-struct fields don't exceed the page size
	LargeList * list_frames = list_init(sysconf(_SC_PAGESIZE)/sizeof(AVFrame *) - LLIST_DATA_OFFSET);

	// List for the histogram differences between the frames of the video 
	LargeList * list_hist_diff = list_init(sysconf(_SC_PAGESIZE)/sizeof(void *) - LLIST_DATA_OFFSET);

	// List for the cuts found by the color histogram approach
	LargeList * list_cuts_colors = list_init(sysconf(_SC_PAGESIZE)/sizeof(void *) - LLIST_DATA_OFFSET);

	while ((pFrame = nextFrame(vidIt, NULL)) != NULL) {
		frameCount++;

		// Allocate a new frame, obviously
		AVFrame* pFrameRGB24 = av_frame_alloc();
		if (!pFrameRGB24) {
			// TODO Errorhandleing / frees
			return -1;
		}
		if (avpicture_alloc((AVPicture *)pFrameRGB24, PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT) < 0) {
			// TODO Errorhandleing / frees
			return -1;
		}
		pFrameRGB24->width = DESTINATION_WIDTH;
		pFrameRGB24->height = DESTINATION_HEIGHT;

		// Convert to a smaller frame for faster processing	
		sws_scale(convert_rgb24, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, vidIt->cctx->height, pFrameRGB24->data, pFrameRGB24->linesize);

		av_frame_free(&pFrame);

		// Push frame on the list
		list_push(list_frames, pFrameRGB24);

		// If one bulk of frames is filled, let the frames be processed first and clear the list
		if (list_frames->size >= TOTAL_FRAMES_IN_MEMORY) {
					// Process frames
					detectCutsByHistogram(list_frames, list_cuts_colors, bulkStart, &feedback_colors, list_hist_diff);

					list_forall(list_frames, (void (*) (void *))avpicture_free);
					list_forall(list_frames, av_free);
					list_destroy(list_frames);

					list_frames = list_init(sysconf(_SC_PAGESIZE)/sizeof(AVFrame *) - LLIST_DATA_OFFSET);
	
					// Set the next bulkStart
					bulkStart = frameCount;
		}
	}

	// Process the remaining frames
	detectCutsByHistogram(list_frames, list_cuts_colors, bulkStart, &feedback_colors, list_hist_diff);

	list_forall(list_frames, (void (*) (void *))avpicture_free);
	list_forall(list_frames, av_free);
	list_destroy(list_frames);

	free(feedback_colors.last_hist);


	int i;
// WIP: Fade/Disolve detection
	int hist_diff_size = list_hist_diff->size;
	uint32_t *hist_diff = (uint32_t *)malloc(sizeof(uint32_t) * hist_diff_size);
	ListIterator *listIt = list_iterate(list_hist_diff);
	for (i = 0; i < hist_diff_size; i++) {
		hist_diff[i] = (uint32_t)(intptr_t)list_next(listIt);
	}

	uint32_t *hist_diff_conv = applyRectWindow(hist_diff, hist_diff_size, 3);

	drawGraph(hist_diff_conv, hist_diff_size, 400, 0.01, 1);
	drawGraph(hist_diff, hist_diff_size, 400, 0.01, 0);

	free(listIt);
	free(hist_diff);
	free(hist_diff_conv);
// */

	// Copy the found cuts into an array
	int cutCount = list_cuts_colors->size;
	*cuts = (uint32_t *)malloc(sizeof(uint32_t) * (cutCount + 1));
	ListIterator *cutsIt = list_iterate(list_cuts_colors);
	for (i = 0; i < cutCount; i++) {
		(*cuts)[i] = (uint32_t)(intptr_t)list_next(cutsIt);
	}
	(*cuts)[i] = frameCount;
	free(cutsIt);


	list_destroy(list_cuts_colors);
	list_destroy(list_hist_diff);

	sws_freeContext(convert_rgb24);
	destroy_VideoIterator(vidIt);

	return cutCount;
}

int main(int argc, char **argv) {	
	// Registers all available codecs
	av_register_all();

	if (argc < 2) {
		printf("Missing parameter!\n");
		return -1;
	}
	uint32_t *cuts;

	int cutCount = processVideo(argv[1], &cuts);

	printf("0");

	int i;
	for (i = 0; i <= cutCount; i++) {
		printf(" %d", cuts[i]);
	}
	printf("\n");
	free(cuts);
	return 0;
}
