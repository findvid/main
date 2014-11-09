#include <unistd.h>

#include "fvutils.h"
#include "largelist.h"
#include "edgedetect.h"
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
		uint32_t e = (uint32_t)v_e;
		uint32_t c = (uint32_t)v_c;
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
		(*array)[i++] = (uint32_t)v_e;
		v_e = list_next(iter_e);
	}
	while(v_c != NULL) {
		(*array)[i++] = (uint32_t)v_c;
		v_c = list_next(iter_c);
	}
	free(iter_e);
	free(iter_c);
	// Return how many different values were actually written into the array
	return i;
}

int findCuts(AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx, int videoStream, uint32_t **cuts) {
	// Allocate frame to read packets into
	AVFrame *pFrame = av_frame_alloc();
	if (!pFrame) {
		// TODO Errorhandleing
		return -1;
	}
	AVFrame *pFrameRGB24 = av_frame_alloc();
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
	
	// Object needed to perform conversions from a source dimension to a destination dimension using certain filters
	// Convert into a smaller frame for easier processing
	struct SwsContext *convert_rgb24 = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	struct SwsContext *convert_g8 = sws_getContext(DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
	
	// Finally, start reading packets from the file
	int frameCount = 0;
	int bulkStart = 0; // Which frame does the current bulk start at?
	int frameFinished = 0;
	AVPacket packet;

	//List to contain all the small frames and pass it to processing when the video is finished or one bulk is filled (defined by MAX_MEMORY_USAGE)
	//Each block of data should be the size of a mempage and contains pointers.
	LargeList * list_frames = list_init(getpagesize()/sizeof(AVFrame *) - LLIST_DATA_OFFSET); //Subtract this value so the list-struct fields don't exceed the page size

	//Another list to store the position of cut frames; a bit ugly, because void pointers store integers this way, but pragmatic
	LargeList * list_cuts_edges = list_init(getpagesize()/sizeof(void *) - LLIST_DATA_OFFSET); //Will likely never have to create a new link with a whole page of cuts
	LargeList * list_cuts_colors = list_init(getpagesize()/sizeof(void *) - LLIST_DATA_OFFSET); //Will likely never have to create a new link with a whole page of cuts

	//Store feedback given by a detection feature and pass it as an argument to the next call to it
	ShotFeedback feedback_edges;
	feedback_edges.lastFrame = NULL;
	feedback_edges.diff = NULL;

	ColorHistFeedback feedback_colors;
	feedback_colors.last_hist = newHistHsv();
	feedback_colors.last_diff = 0;
	feedback_colors.last_derivation = 0;

	// Mind that we read from pFormatCtx, which is the general container file...
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		// ... therefore, not every packet belongs to our video stream!
		if (packet.stream_index == videoStream) {
			// Packet is part of the video stream previously found
			// therefore use the CodecContext previously retrieved with
			// avcodec_find_decoder2 to read the packet into pFrame
			if (avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet) < 0) {
				// TODO Errorhandleing / frees
				return -1;
			}
			// frameFinished is set be avcodec_decode_video2 accordingly
			if (frameFinished) {
				frameCount++;
				// Convert to a smaller frame for faster processing	
				sws_scale(convert_rgb24, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB24->data, pFrameRGB24->linesize);
				
				// TODO Remove this: printf("Push frame@0x%x\n", pFrameRGB24);
				
				list_push(list_frames, pFrameRGB24);

				//!!!!! Allocate a new frame, obviously
				pFrameRGB24 = av_frame_alloc();
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


				// If one bulk of frames is filled, let the frames be processed first and clear the list
				if (list_frames->size >= TOTAL_FRAMES_IN_MEMORY) {
					// HERE THERE BE PROCESSING
					printf("Process bulk of %d frames...\n", list_frames->size);
					// call a method to fill list_cuts with detected cut frames
					// old feedback-array is freed by the function
					//detectCutsByEdges(list_frames, list_cuts_edges, bulkStart, &feedback_edges, convert_g8, DESTINATION_WIDTH, DESTINATION_HEIGHT);
					if (feedback_edges.lastFrame != NULL) av_free(feedback_edges.lastFrame);
					feedback_edges.lastFrame = list_pop(list_frames);


					detectCutsByHistogram(list_frames, list_cuts_colors, bulkStart, &feedback_colors);
					// Get rid of the old frames and destroy the list
					list_forall(list_frames, av_free);
					list_destroy(list_frames);
					// ... and get a new one. A potential list_clear wouldn't do much else, still, there are possibly some ways to do this more gracefully
					list_frames = list_init(getpagesize()/sizeof(AVFrame *) - LLIST_DATA_OFFSET); //Subtract this value so the list-struct fields don't exceed the page size
					
					bulkStart = frameCount;
				}
			}

		}
		av_free_packet(&packet);
	}

	// Process the (remaining) frames
					
	// call a method to fill list_cuts with detected cut frames
	// detectCutsByEdges(list_frames, list_cuts_edges, bulkStart, &feedback_edges, convert_g8, DESTINATION_WIDTH, DESTINATION_HEIGHT);
	detectCutsByHistogram(list_frames, list_cuts_colors, bulkStart, &feedback_colors);

	// Final feedback goes nowhere
	free(feedback_edges.diff);
	av_free(feedback_edges.lastFrame);

	free(feedback_colors.last_hist);

	list_forall(list_frames, av_free);
	list_destroy(list_frames);

	int cutCount = mergeListsToArray(list_cuts_edges, list_cuts_colors, cuts);

	list_destroy(list_cuts_edges);
	list_destroy(list_cuts_colors);

	av_free(pFrame);
	sws_freeContext(convert_rgb24);
	sws_freeContext(convert_g8);
	return cutCount;
}

int processVideo(char *filename, uint32_t **cuts) {
	av_register_all();
	// Struct holding information about container-file
	AVFormatContext *pFormatCtx = NULL;

	// Read format information into struct
	if(avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0) {
		// TODO Errorhandleing
		return -1;
	}
	
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		// TODO Errorhandleing
		return -1; // Couldn't find stream information
	}
	
	// Search for a video stream, assuming there's only one, we pick the first and continue
	int videoStream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++ ) {
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	}
	
	// Could not find a video stream
	if (videoStream == -1) {
		// TODO Errorhandleing
		return -1;
	}

	// Retrieve CodecContext
	AVCodecContext *pCodecCtx = pFormatCtx->streams[videoStream]->codec;
	
	// Get the appropiate Decoder for the video stream
	AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		// TODO Errorhandleing
		printf("Unsupported Codec\n");
		return -1;
	}
	
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		// TODO Errorhandleing
		return -1;
	}

	// >>>>> DO PROCESSING HERE <<<<<
	int cutCount = findCuts(pFormatCtx, pCodecCtx, videoStream, cuts);

	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	if (cutCount < 0) {
		// TODO Errorhandleing / frees
		return -1;
	}
	//printf("RESULTS:\n");
	//for (int i = 0; i < c_cuts; i++) printf("%d\n", cuts[i]);
	// Better do an extra catch here in case there was a 0 on the list for some reason
	return cutCount;
}
/*
int main(int argc, char **argv) {	
	// Registers all available codecs
	av_register_all();

	if (argc < 2) {
		printf("Missing parameter!\n");
		return -1;
	}
	// TODO Do things
	uint32_t *cuts;
	int cutCount = processVideo(argv[1], &cuts);
	
	printf("RESULTS (%d, %p):\n", cutCount, cuts);
	for (int i = 0; i < cutCount; i++) printf("%d\n", cuts[i]);
	return 0;
}*/
