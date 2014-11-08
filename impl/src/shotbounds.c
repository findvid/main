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


int main(int argc, char **argv) {	
	// Registers all available codecs
	av_register_all();

	// Struct holding information about container-file
	AVFormatContext * pFormatCtx = NULL;

	if (argc < 2) {
		printf("Missing parameter!\n");
		return -1;
	}

	// Read format information into struct
	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
		return -1;
	
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1; // Couldn't find stream information
	
	// Search for a video stream, assuming there's only one, we pick the first and continue
	int videoStream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++ )
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	
	// Could not find a video stream
	if (videoStream == -1)
		return -1;

	// Retrieve CodecContext
	AVCodecContext *pCodecCtx = pFormatCtx->streams[videoStream]->codec;
	
	// Get the appropiate Decoder for the video stream
	AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Unsupported Codec\n");
		return -1;
	}
	
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		return -1;
	}

	// Allocate frame to read packets into
	AVFrame *pFrame = av_frame_alloc();
	AVFrame *pFrameRGB24 = av_frame_alloc();

	if (pFrame == NULL || pFrameRGB24 == NULL) 
		return -1;

	
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT);
	uint8_t *buffer_rgb24 = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)pFrameRGB24, buffer_rgb24, PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT);

	// Object needed to perform conversions from a source dimension to a destination dimension using certain filters
	//Convert into a smaller frame for easier processing
	struct SwsContext *convert_rgb24 = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	struct SwsContext *convert_g8 = sws_getContext(DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
	
	// Finally, start reading packets from the file
	int frameCount = 0;
	int frameBulk = 0; // Which frame does the current bulk start at?
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
	//ShotFeedback feedback_colors;
	//feedback_colors.lastFrame = NULL;
	//feedback_colors.diff = NULL;
	//should be copied into the new internal var-array and then be freed by the feature detection

	// Mind that we read from pFormatCtx, which is the general container file...
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		// ... therefore, not every packet belongs to our video stream!
		if (packet.stream_index == videoStream) {
			// Packet is part of the video stream previously found
			// therefore use the CodecContext previously retrieved with
			// avcodec_find_decoder2 to read the packet into pFrame
			int len = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if (len<0) return -1;
			// frameFinished is set be avcodec_decode_video2 accordingly
			if (frameFinished) {
				frameCount++;
				//Convert to a smaller frame for faster processing	
				sws_scale(convert_rgb24, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB24->data, pFrameRGB24->linesize);
				
				//printf("Push frame@0x%x\n", pFrameRGB24);
				
				list_push(list_frames, pFrameRGB24);


				//!!!!! Allocate a new frame, obviously
				pFrameRGB24 = av_frame_alloc();
				numBytes = avpicture_get_size(PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT);
				buffer_rgb24 = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
				avpicture_fill((AVPicture *)pFrameRGB24, buffer_rgb24, PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT);
				pFrameRGB24->width = DESTINATION_WIDTH;
				pFrameRGB24->height = DESTINATION_HEIGHT;


				//If one bulk of frames is filled, let the frames be processed first and clear the list
				if (list_frames->size >= TOTAL_FRAMES_IN_MEMORY) {
					//HERE THERE BE PROCESSING
					printf("Process bulk of %d frames...\n", list_frames->size);
					frameBulk = frameCount;
					//call a method to fill list_cuts with detected cut frames
					//old feedback-array is freed by the function
					//detectCutsByEdges(list_frames, list_cuts_edges, frameBulk, &feedback_edges, convert_g8, DESTINATION_WIDTH, DESTINATION_HEIGHT);
					
					//do something similiar for color histograms
					//your move				
					detectCutsByHistogram(list_frames, list_cuts_colors, 0, &feedback_colors);
					
					if (feedback_edges.lastFrame != NULL) av_free(feedback_edges.lastFrame);
					//if (feedback_colors.lastFrame != NULL) av_free(feedback_colors.lastFrame);
					feedback_edges.lastFrame = list_pop(list_frames);
					//feedback_colors.lastFrame = feedback_edges.lastFrame;

					//Get rid of the old frames and destroy the list
					list_forall(list_frames, av_free);
					list_destroy(list_frames);
					// ... and get a new one. A potential list_clear wouldn't do much else, still, there are possibly some ways to do this more gracefully
					list_frames = list_init(getpagesize()/sizeof(AVFrame *) - LLIST_DATA_OFFSET); //Subtract this value so the list-struct fields don't exceed the page size
					
					frameBulk = frameCount;
				}
			}

		}
		av_free_packet(&packet);
	}

	//Process the (remaining) frames
					
	//call a method to fill list_cuts with detected cut frames
	//detectCutsByEdges(list_frames, list_cuts_edges, frameBulk, &feedback_edges, convert_g8, DESTINATION_WIDTH, DESTINATION_HEIGHT);
	
	//do something similiar for color histograms
	//your move
	detectCutsByHistogram(list_frames, list_cuts_colors, frameBulk, &feedback_colors);

ListIterator *lit = list_iterate(list_cuts_colors);
void* fuu = list_next(lit);
//WHY THE FOX DO THE NEXT THREE LINES AFFECT THE RESULT OUTPUT AAAAHHHH I THINK I'M GETTING MAD! TIME TO SLEEP!!!!11111elf
//printf("Items: %d\n", list_cuts_colors->items);
//printf("Size: %d\n", list_cuts_colors->size);
//printf("Cap: %d\n", list_cuts_colors->capacity);
while (fuu) {
	printf("LOL: %d\n", (uint32_t) fuu);
	fuu = list_next(lit);
}
	//Final feedback goes nowhere
	free(feedback_edges.diff);
	//free(feedback_colors.diff);
	free(feedback_colors.last_hist);
	av_free(feedback_edges.lastFrame);
	//av_free(feedback_colors.lastFrame);

	list_forall(list_frames, av_free);
	list_destroy(list_frames);
	
	//Finally, sort the cuts and return it as one coherent array
	uint32_t c_cuts = (list_cuts_edges->size + list_cuts_colors->size);
	uint32_t * cuts = calloc(sizeof(uint32_t), c_cuts);
	
	int i = 0;
	void * v_e;
	void * v_c;
	ListIterator * iter_e = list_iterate(list_cuts_edges);
	ListIterator * iter_c = list_iterate(list_cuts_colors);
printf("E: %p, C: %p\n", iter_e, iter_c);
	v_e = list_next(iter_e);
	v_c = list_next(iter_c);
	while ((v_e != NULL) && (v_c != NULL)) {
		uint32_t e = (uint32_t)v_e;
		uint32_t c = (uint32_t)v_c;
		if ( e == c ) {
			cuts[i++] = c;
			v_e = list_next(iter_e);
			v_c = list_next(iter_c);
		} else if (e < c) {
			cuts[i++] = e;
			v_e = list_next(iter_e);
		} else {
			cuts[i++] = c;
			v_c = list_next(iter_c);
		}
	}

	while(v_e != NULL) {
		cuts[i++] = (uint32_t)v_e;
		v_e = list_next(iter_e);
	}

	while(v_c != NULL) {
		cuts[i++] = (uint32_t)v_c;
		v_c = list_next(iter_c);
	}

	list_destroy(list_cuts_edges);
	list_destroy(list_cuts_colors);

	av_free(pFrame);
	sws_freeContext(convert_rgb24);
	sws_freeContext(convert_g8);

	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	//Still in main method, let's just pretend this goes anywhere
	printf("RESULTS:\n");
	for (int i = 0; i < c_cuts; i++) printf("%d\n", cuts[i]);
	return (int)cuts; //Must return a struct for to contain array boundary aswell...
}
