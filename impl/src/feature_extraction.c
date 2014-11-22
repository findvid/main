#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>
#include <sys/stat.h>

#include "feature_extraction.h"
#include "largelist.h"


void dummyFeatureLength(int * l) {
	*l = 5;
}

void dummyFeature(AVFrame * frame, uint32_t ** data, int position) {
	data[position] = malloc(sizeof(uint32_t) * 5);
	data[position][0] = *(((uint32_t *)frame->data[0] + 0));
	data[position][1] = *(((uint32_t *)frame->data[0] + 1));
	data[position][2] = *(((uint32_t *)frame->data[0] + 2));
	data[position][3] = *(((uint32_t *)frame->data[0] + 3));
	data[position][4] = *(((uint32_t *)frame->data[0] + 4));
}

//Extract the videos name without extension from the given path
char * getVideoname(char *path) {
	int start, end;
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

FeatureTuple * getFeatures(char * filename, char * expath, int vidThumb, uint32_t * sceneFrames, int sceneCount) {
	av_register_all();
	FeatureTuple * res = malloc(sizeof(FeatureTuple));

	res->feature_list = malloc(sizeof(uint32_t **) * FEATURE_AMNT);
	res->feature_list[0] = malloc(sizeof(uint32_t *) * sceneCount);
	res->feature_list[1] = malloc(sizeof(uint32_t *) * sceneCount);
	res->feature_list[2] = malloc(sizeof(uint32_t *) * sceneCount);
	res->feature_list[3] = malloc(sizeof(uint32_t *) * sceneCount);

	res->feature_length = malloc(sizeof(uint32_t) * FEATURE_AMNT);
	dummyFeatureLength(&res->feature_length[0]); 
	dummyFeatureLength(&res->feature_length[1]); 
	dummyFeatureLength(&res->feature_length[2]); 
	dummyFeatureLength(&res->feature_length[3]); 
	//res->feature_count = sceneCount;
	res->feature_count = 0; //If nothing's done, there are no features saved in res->feature_list[x][y]


	char * videoName = getVideoname(filename);
	char thumbnailFilename[256]; //Pre alloc some space for full filenames to sprintf to

	VideoIterator * iter = get_VideoIterator(filename);

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

	sprintf(folder, "%s/%s", expath, videoName);

	if (mkdir(folder, 0777) < 0) {
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
	int writtenFrames = 0;
	int hadVidThumb = 0;
	readFrame(iter, frame, &gotFrame);
	while (gotFrame) {
		if (currentFrame == vidThumb) {
			//Just save a thumbnail for the video
			hadVidThumb = 1;
			//Encode frame into buffer
			frame->pts = writtenFrames++;
			int written = avcodec_encode_video(trgtCtx, buffer, numBytes, frame);

			sprintf(thumbnailFilename, "%s/video.jpeg", folder);
			FILE * thumbFile = fopen(thumbnailFilename, "wb");
			if (!thumbFile) {
				printf("Cannot open file to save thumbnail to!(%s)\n", thumbnailFilename);
				free(res);
				return NULL;
			}

			fwrite(buffer, 1, written, thumbFile);
			fclose(thumbFile);
			
		}
		if (currentFrame == sceneFrames[currentScene]) {
			//First save the thumbnail
			
			//Encode frame into buffer
			frame->pts = writtenFrames++;
			int written = avcodec_encode_video(trgtCtx, buffer, numBytes, frame);

			sprintf(thumbnailFilename, "%s/scene%d.jpeg", folder, currentScene);
			FILE * thumbFile = fopen(thumbnailFilename, "wb");
			if (!thumbFile) {
				printf("Cannot open file to save thumbnail to!(%s)\n", thumbnailFilename);
				free(res);
				return NULL;
			}

			fwrite(buffer, 1, written, thumbFile);
			fclose(thumbFile);
			
			currentScene++;

			//Get features from different components for this frame
			//Do some M.A.G.I.C.
			//getMagicalRainbowFeatures(frame, res->feature_list[0], currentScene);
			//...
			dummyFeature(frame, res->feature_list[0], currentScene);
			dummyFeature(frame, res->feature_list[1], currentScene);
			dummyFeature(frame, res->feature_list[2], currentScene);
			dummyFeature(frame, res->feature_list[3], currentScene);
		}
		if (currentScene >= sceneCount && hadVidThumb) break; //Everything's done
		currentFrame++;
		readFrame(iter, frame, &gotFrame);
	}
	av_frame_free(&frame);
	av_free(buffer);
	free(videoName);
	destroy_VideoIterator(iter);
	avcodec_close(trgtCtx);
	avcodec_free_context(&trgtCtx);
	return res;
}

void destroyFeatures(FeatureTuple * t) {
	for (int i = 0; i < FEATURE_AMNT; i++) {
		for(int j = 0; j < t->feature_count; j++)
			free(t->feature_list[i][j]);
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
*/
