#include "fvutils.h"

AVFrame * copyFrame(AVFrame *pPic, struct SwsContext * ctx, int width, int height) {
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_RGB24, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_RGB24, width, height);
	sws_scale(ctx, (const uint8_t * const*)pPic->data, pPic->linesize, 0, height, res->data, res->linesize);
	return res;
}


void SaveFrameRGB24(AVFrame *pFrame, int width, int height, int i) {
	FILE *pFile;
	char szFilename[32];
	int  y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", i);
	pFile=fopen(szFilename, "wb");
	if(pFile==NULL)
		return;

	// Print header for .ppm-format
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);   
	// Write bytevectors into file
	for(y = 0; y < height; y++)
		fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
								  
	// Close file
	fclose(pFile);
}


void SaveFrameG8(AVFrame * pFrame, int width, int height, int i) {
	FILE *pFile;
	char szFilename[32];

	// Open file
	sprintf(szFilename, "frame%d.ppm", i);
	pFile=fopen(szFilename, "wb");
	if(pFile==NULL)
		return;

	// Print header for .ppm-format
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);   
	// Write bytevectors into file
	uint8_t lilbuff[3];
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++ ) {
			lilbuff[0] = pFrame->data[0][x + y * pFrame->linesize[0]];
			lilbuff[1] = pFrame->data[0][x + y * pFrame->linesize[0]];
			lilbuff[2] = pFrame->data[0][x + y * pFrame->linesize[0]];
			fwrite(lilbuff, 1, 3, pFile);
		}
	} 
	// Close file
	fclose(pFile);
}


typedef struct {
	uint8_t *data;
	int w;
	int h;
} Image;

int saveImage(Image *img, char *filename) {
	FILE *pFile;
	pFile = fopen(filename, "wb");
	if (pFile == NULL) {
		return -1;
	}
	
	fprintf(pFile, "P6\n%d %d\n255\n", img->w, img->h);
	fwrite(img->data, 3*img->w*img->h, 1, pFile);
	fclose(pFile);
	return 0;
}

int drawGraph(uint32_t *data, int len, int height, double scale, int nr) {
	Image graph;
	graph.data = (uint8_t *)calloc(len * 3 * height, sizeof(uint8_t));
	graph.w = len;
	graph.h = height;
	int x;
	int y;
	for (x = 0; x < len; x++) {
		uint32_t value = ((double)data[x]) * scale;
	//	printf("Draw (%d,%d)\n", x, value);
		for (y = 1; (y <= value) && (y <= height); y++) {
			graph.data[((height - y) * len + x) * 3] = 255;
		}
	}

	char filename[32];
	sprintf(filename, "graph%d.ppm", nr);
	saveImage(&graph, filename);
	free(graph.data);
	return 0;
}

VideoIterator * get_VideoIterator(const char * filename) {
	VideoIterator * iter = (VideoIterator *)malloc(sizeof(VideoIterator));

	iter->fctx = NULL;
	iter->cctx = NULL;

	if(avformat_open_input(&iter->fctx, filename, NULL, NULL) != 0)
		goto failure;
	if(avformat_find_stream_info(iter->fctx, NULL) < 0)
		goto failure;

	iter->videoStream = -1;
	for (int i = 0; i < iter->fctx->nb_streams; i++ )
		if (iter->fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			iter->videoStream = i;
			break;
		}

	if (iter->videoStream == -1) {
		goto failure;
	}

	iter->cctx = iter->fctx->streams[iter->videoStream]->codec;
	AVCodec * pCodec = avcodec_find_decoder(iter->cctx->codec_id);
	if (pCodec == NULL)
		goto failure;
	
	if (avcodec_open2(iter->cctx, pCodec, NULL) < 0)
		goto failure;

	return iter;

failure:
	free(iter);
	return NULL;
}

AVFrame * nextFrame(VideoIterator * iter, int * gotFrame) {
	//Possibly omitt this and force passing an actual pointer because this should always be checked...technically.
	int gotFrameInt;
	if (gotFrame == NULL) {
		gotFrame = &gotFrameInt;
	}
	*gotFrame = 0;

	AVFrame * res = av_frame_alloc();
	
	readFrame(iter, res, gotFrame);
	if (!*gotFrame) {
		av_frame_free(&res);
		return NULL;
	}
	return res;
}

void readFrame(VideoIterator * iter, AVFrame * targetFrame, int * gotFrame) {
	//Possibly omitt this and force passing an actual pointer because this should always be checked...technically.
	int gotFrameInt;
	if (gotFrame == NULL) {
		gotFrame = &gotFrameInt;
	}
	*gotFrame = 0;

	AVPacket p;
	while (!*gotFrame) {
		if (av_read_frame(iter->fctx, &p) < 0) {
			*gotFrame = 0;
			av_free_packet(&p);
			return;
		}
		if (p.stream_index == iter->videoStream) {
			int len = avcodec_decode_video2(iter->cctx, targetFrame, gotFrame, &p);
			if (len<0) {
				*gotFrame = 0;
				av_free_packet(&p);
				return;
			}
			/*
			if (!*gotFrame) {
				//Save the packet and frame and return the frame as does decode
				av_copy_packet(iter->packet, &p);
				iter->frame = res;
				return res;
			} else if(iter->packet != NULL) {
				//Clean up Iterator from feedback vars
				av_free_packet(iter->packet);
			}*/
		}
		av_free_packet(&p);
	}
}

void destroy_VideoIterator(VideoIterator * iter) {
	avcodec_close(iter->cctx);
	avformat_close_input(&iter->fctx);
	free(iter);
}

double getFramerate(const char * filename) {
	av_register_all();
	VideoIterator * iter = get_VideoIterator(filename);
	//framerate is not a member of struct AVCodecContext ???!!! Similar to SwsContext not appearing to have dstw or dsth...
	//double res = (double)iter->cctx->framerate.num / iter->cctx->framerate.den;
	AVRational * r = &(iter->fctx->streams[iter->videoStream]->avg_frame_rate);
	double res = (double)r->num / r->den;
	//double res = (double)iter->cctx->time_base.den / iter->cctx->time_base.num;
	destroy_VideoIterator(iter);
	return res;
}
