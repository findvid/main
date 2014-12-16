#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>
#include <sys/stat.h>

#include "feature_extraction.h"
#include "largelist.h"
#include "histograms.h"
#include "edgedetect.h"

#define DESTINATION_WIDTH 320
#define DESTINATION_HEIGHT 200

#define TINY_IMAGE_WIDTH 16
#define TINY_IMAGE_HEIGHT 16


void tinyImageLength(uint32_t * l) {
	// 3 because of the 3 values H, S, V
	*l = TINY_IMAGE_WIDTH * TINY_IMAGE_HEIGHT * 3;
}

void tinyImageFeature(AVFrame * frame, uint32_t ** data, struct SwsContext * convert_tiny) {
	// 3 because of the 3 values H, S, V
	*data = (uint32_t *)calloc(TINY_IMAGE_WIDTH * TINY_IMAGE_HEIGHT * 3, sizeof(uint32_t));

	AVFrame * frameTiny = av_frame_alloc();
	if (!frameTiny) {
		// TODO Errorhandleing / frees
		return;
	}
	if (avpicture_alloc((AVPicture *)frameTiny, PIX_FMT_RGB24, TINY_IMAGE_WIDTH, TINY_IMAGE_HEIGHT) < 0) {
		// TODO Errorhandleing / frees
		return;
	}

	// Convert to tiny image     
	sws_scale(convert_tiny, (const uint8_t* const*)frame->data, frame->linesize, 0, DESTINATION_HEIGHT, frameTiny->data, frameTiny->linesize);

	// TODO Stuff
	int i;
	for (i = 0; i < TINY_IMAGE_WIDTH * TINY_IMAGE_HEIGHT; i++) {
		HSV hsv;
		rgbToHsv(&hsv, frameTiny->data[0][i*3], frameTiny->data[0][i*3+1], frameTiny->data[0][i*3+2]);
		(*data)[i*3] = hsv.h;
		(*data)[i*3+1] = hsv.s;
		(*data)[i*3+2] = hsv.v;
	}

	avpicture_free((AVPicture *)frameTiny);
	av_frame_free(&frameTiny);
}

void histogramLength(uint32_t * l) {
	*l = 128;
}

void histogramFeature(AVFrame * frame, uint32_t ** data) {
	*data = (uint32_t *)calloc(128, sizeof(uint32_t));
	fillHistHsv(*data, frame);
}

void dummyFeatureLength(uint32_t * l) {
	*l = 5;
}

void dummyFeature(AVFrame * frame, uint32_t ** data) {
	*data = (uint32_t *)malloc(sizeof(uint32_t) * 5);
	(*data)[0] = *(((uint32_t *)frame->data[0] + 0));
	(*data)[1] = *(((uint32_t *)frame->data[0] + 1));
	(*data)[2] = *(((uint32_t *)frame->data[0] + 2));
	(*data)[3] = *(((uint32_t *)frame->data[0] + 3));
	(*data)[4] = *(((uint32_t *)frame->data[0] + 4));
}

//Extract the videos name without extension from the given path
char * getVideoname(const char *path) {
	int start = 0, end = 0;
	int pos = 0;
	while (path[pos] != '\0') {
		if (path[pos] == '/') start = pos+1;
		if (path[pos] == '.') end = pos;
		pos++;
	}
	char * res = malloc(sizeof(char) * (end - start + 1));
	pos = start;
	int i = 0;
	while (pos < end)
		res[i++] = path[pos++];
	
	res[i] = '\0';
	return res;
}

// TODO: Probably should be moved to fvutils
/*
 * Save a video frame to a file
 *
 * @param filename	path to where the frame should be saved
 * @param avctx		AVCodecContext containing information about the frame
 * @param frame		Frame to be saved
 *
 * @return		0 on success, negative value on failure
 */
int writeFrame(const char * filename, AVCodecContext * avctx, const AVFrame * frame) {
	int got_packet;
	AVPacket avpkt;
	av_init_packet(&avpkt);
	avpkt.data = NULL;
	avpkt.size = 0;
	//Encode frame into buffer
	int ret = avcodec_encode_video2(avctx, &avpkt, frame, &got_packet);
	if (ret != 0) {
		printf("avcodec_encode_video2 failed :-(\n");
	}

	FILE * thumbFile = fopen(filename, "wb");
	if (!thumbFile) {
		printf("Cannot open file to save thumbnail to!(%s)\n", filename);
		return -1;
	}

	fwrite(avpkt.data, 1, avpkt.size, thumbFile);
	fclose(thumbFile);
	av_free_packet(&avpkt);
	return 0;
}

FeatureTuple * getFeatures(const char * filename, const char * hashstring, const char * expath, int vidThumb, uint32_t * sceneFrames, int sceneCount) {
	av_register_all();
	FeatureTuple * res = malloc(sizeof(FeatureTuple));

	res->feature_list = malloc(sizeof(uint32_t **) * FEATURE_AMNT);
	int i;
	for (i = 0; i < FEATURE_AMNT; i++) {
		res->feature_list[i] = malloc(sizeof(uint32_t *) * sceneCount);
	}

	res->feature_length = malloc(sizeof(uint32_t) * FEATURE_AMNT);
	tinyImageLength(&res->feature_length[0]); 
	edgeFeatures_length(&res->feature_length[1]); 
	histogramLength(&res->feature_length[2]); 
	dummyFeatureLength(&res->feature_length[3]); 
	res->feature_count = sceneCount;
	//res->feature_count = 0; //If nothing's done, there are no features saved in res->feature_list[x][y]


	//char * videoName = getVideoname(filename);
	char thumbnailFilename[256]; //Pre alloc some space for full filenames to sprintf to

	VideoIterator * iter = get_VideoIterator(filename);

	// Get Sws context to downscale the frames
	struct SwsContext * convert_rgb24 = sws_getContext(iter->cctx->width, iter->cctx->height, iter->cctx->pix_fmt, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	

	// SWS Context to convert the downscaled frame grayscales
	struct SwsContext * convert_g8 = sws_getContext(DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
	//Interpolated mask of weights to distribute edges
	InterpolationWeights * edgeWeights = getLinearInterpolationWeights(DESTINATION_WIDTH, DESTINATION_HEIGHT);
	
	// SWS Context to convert the downscaled frame to a tiny image
	struct SwsContext * convert_tiny = sws_getContext(DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, TINY_IMAGE_WIDTH, TINY_IMAGE_HEIGHT, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	//Get a target codec to write to JPEG files
	AVCodecContext * trgtCtx = avcodec_alloc_context3(NULL);
	if (trgtCtx == NULL) {
		printf("Failed to allocate a new CodecContext!\n");
		free(res);
		return NULL;
	}

	trgtCtx->bit_rate = iter->cctx->bit_rate;
	trgtCtx->width = iter->cctx->width;
	trgtCtx->height = iter->cctx->height;
	trgtCtx->pix_fmt = PIX_FMT_YUVJ420P;
	trgtCtx->codec_id = CODEC_ID_MJPEG;
	trgtCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	trgtCtx->time_base.num = iter->cctx->time_base.num;
	trgtCtx->time_base.den = iter->cctx->time_base.den;

	trgtCtx->mb_lmin = (trgtCtx->lmin = trgtCtx->qmin * 1);
	trgtCtx->mb_lmax = (trgtCtx->lmax = trgtCtx->qmax * 1);
	trgtCtx->flags = CODEC_FLAG_QSCALE;
	trgtCtx->global_quality = trgtCtx->qmin * 1;


	AVCodec * targetCodec = avcodec_find_encoder(trgtCtx->codec_id);
	if(targetCodec == NULL) {
		printf("Could not initialize target codec\n");
		free(res);
		return NULL;
	}

	if (avcodec_open2(trgtCtx, targetCodec, NULL) < 0) {
		printf("Could not open target codec\n");
		free(res);
		return NULL;
	}

	//Create the folder for this video under a set path
	char folder[256];

	sprintf(folder, "%s/%s", expath, hashstring);

	if (mkdir(folder, 0777) < 0) {
		// TODO EEXIST dosen't check if it's a folder
		if (errno != EEXIST) {
			printf("Cannot create folder to save to!(%s)\nERROR = %d\n", folder, errno);
			free(res);
			return NULL;
		} //clean folder with a shell call first?
	}

	//Allocate source frame for the iterator to decode into
	AVFrame * frame = av_frame_alloc();
	//Allocate Target buffer
	int numBytes = avpicture_get_size(PIX_FMT_YUVJ420P, iter->cctx->width, iter->cctx->height);
	uint8_t * buffer = av_malloc(numBytes);

	frame->pts = 0;
	frame->quality = trgtCtx->global_quality;
	
	int gotFrame = 0;
	int currentFrame = 0;
	int currentScene = 0;
	//int writtenFrames = 0;
	int hadVidThumb = 0;
	readFrame(iter, frame, &gotFrame);
	while (gotFrame) {
		if (currentFrame == vidThumb) {
			//Just save a thumbnail for the video
			hadVidThumb = 1;
			sprintf(thumbnailFilename, "%s/video.jpeg", folder);
			writeFrame(thumbnailFilename, trgtCtx, frame);
		}
		if (currentFrame == sceneFrames[currentScene]) {
			//First save the thumbnail
			
			sprintf(thumbnailFilename, "%s/scene%d.jpeg", folder, currentScene);

			writeFrame(thumbnailFilename, trgtCtx, frame);

			AVFrame * pFrameRGB24 = av_frame_alloc();
			if (!pFrameRGB24) {
				// TODO Errorhandleing / frees
				return NULL;
			}
			if (avpicture_alloc((AVPicture *)pFrameRGB24, PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT) < 0) {
				// TODO Errorhandleing / frees
				return NULL;
			}


			// Convert to a smaller frame for faster processing     
			sws_scale(convert_rgb24, (const uint8_t* const*)frame->data, frame->linesize, 0, iter->cctx->height, pFrameRGB24->data, pFrameRGB24->linesize);

			pFrameRGB24->width = DESTINATION_WIDTH;
			pFrameRGB24->height = DESTINATION_HEIGHT;
			
			AVFrame * pFrameG8 = getEdgeProfile(pFrameRGB24, convert_g8, DESTINATION_WIDTH, DESTINATION_HEIGHT);
			

			//Get features from different components for this frame
			//Do some M.A.G.I.C.
			//getMagicalRainbowFeatures(frame, res->feature_list[0], currentScene);
			//...
			tinyImageFeature(pFrameRGB24, &(res->feature_list[0][currentScene]), convert_tiny);
			edgeFeatures(pFrameG8, &(res->feature_list[1][currentScene]), edgeWeights);
			histogramFeature(pFrameRGB24, &(res->feature_list[2][currentScene]));
			dummyFeature(frame, &(res->feature_list[3][currentScene]));

			currentScene++;
			
			avpicture_free((AVPicture *)pFrameG8);
			av_frame_free(&pFrameG8);

			avpicture_free((AVPicture *)pFrameRGB24);
			av_frame_free(&pFrameRGB24);
		}
		if (currentScene > sceneCount && hadVidThumb) break; //Everything's done
		currentFrame++;
		readFrame(iter, frame, &gotFrame);
	}
	if (currentScene < sceneCount) { //Went all the way through without getting all the keyframes; artificially cut off the empty rest of res->features
		res->feature_count -= (sceneCount - currentScene);
	}
	av_frame_free(&frame);
	av_free(buffer);
	destroy_VideoIterator(iter);
	avcodec_close(trgtCtx);
	avcodec_free_context(&trgtCtx);
	sws_freeContext(convert_rgb24);
	sws_freeContext(convert_g8);
	free(edgeWeights->c);
	free(edgeWeights);
	sws_freeContext(convert_tiny);
	return res;
}

void destroyFeatures(FeatureTuple * t) {
	for (int i = 0; i < FEATURE_AMNT; i++) {
		for(int j = 0; j < t->feature_count; j++) {
			free(t->feature_list[i][j]);
		}
		free(t->feature_list[i]);
	}
	free(t->feature_list);
	free(t->feature_length);
	free(t);
}
/*
int main(int argc, char **argv) {
	uint32_t d[5] = {5, 50, 150, 250, 450};
	FeatureTuple * r = getFeatures(argv[1], argv[2], 50, d, 5);

	destroyFeatures(r);
}
// */
