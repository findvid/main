#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>
#include <sys/stat.h>

//Extract the videos name without extension from the given path
char * getVideoName(char *path) {
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

int main(int argc, char** argv) {
	av_register_all();
	AVFormatContext * pFormatCtx = NULL;

	if (argc < 3) {
		printf("Missing parameter! Define a videopath and at least 1 frame number!\n");
		return -1;
	}

	char * videoName = getVideoName(argv[1]);
	char thumbnailFilename[256];	

	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
		printf("Failed to open file!\n");
		return -1;
	}

	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
		printf("Coul not find stream info!\n");
		return -1; // Couldn't find stream information
	}

	int videoStream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++ )
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	
	if (videoStream == -1)
		return -1;

	AVCodecContext *pCodecCtx = pFormatCtx->streams[videoStream]->codec;
	AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Unsupported Codec!\n");
		return -1;
	}
	
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		return -1;
	}

	AVFrame *pFrame = av_frame_alloc();


	if (pFrame == NULL) {
		printf("malloc() for AVFrame failed, cannot read further.\n");
		return -1;
	}

	//Get a target codec to write to JPEG files
	AVCodecContext * trgtCtx = avcodec_alloc_context3(NULL);
	if (trgtCtx == NULL) {
		printf("Failed to allocate a new CodecContext!\n");
		return -1;
	}

	trgtCtx->bit_rate = pCodecCtx->bit_rate;
	trgtCtx->width = pCodecCtx->width;
	trgtCtx->height = pCodecCtx->height;
	trgtCtx->pix_fmt = PIX_FMT_YUVJ420P;
	trgtCtx->codec_id = CODEC_ID_MJPEG;
	trgtCtx->codec_type = AVMEDIA_TYPE_VIDEO;
	trgtCtx->time_base.num = pCodecCtx->time_base.num;
	trgtCtx->time_base.den = pCodecCtx->time_base.den;

	trgtCtx->mb_lmin = (trgtCtx->lmin = trgtCtx->qmin * 1);
	trgtCtx->mb_lmax = (trgtCtx->lmax = trgtCtx->qmax * 1);
	trgtCtx->flags = CODEC_FLAG_QSCALE;
	trgtCtx->global_quality = trgtCtx->qmin * 1;


	AVCodec * targetCodec = avcodec_find_encoder(trgtCtx->codec_id);
	if(targetCodec == NULL) {
		printf("Could not initialize target codec\n");
		return -1;
	}

	if (avcodec_open2(trgtCtx, targetCodec, NULL) < 0) {
		printf("Could not open target codec\n");
		return -1;	
	}

	//Create the folder for this video under a set path
	char folder[256];
	char *p = NULL;

	int path_override = 0;
	strtol(argv[2], &p, 10);
	if (*p != '\0') {
		//This is the path override, as non-digits were found in argv[2]
		sprintf(folder, "%s/%s", argv[2], videoName);
		path_override = 1;
	} else
		sprintf(folder, "/video/videosearch/findvid/thumbnails/%s", videoName); //Default
	
	if (mkdir(folder, 0777) < 0) {
		if (errno != EEXIST) {
			printf("Cannot create folder to save to!(%s)\nERROR = %d\n", folder, errno);
			return -1;
		}
	}

	int frameCount = 0;
	int frameFinished = 0;
	
	int videothumb = strtol(argv[2 + path_override], &p, 10);
	if (*p != '\0')
		fprintf(stderr, "Arg for videothumbnail seems malformed!\n");

	int argpos = 3 + path_override; //0 = cmd, 1 = videopath, 2,3,4,5,6,...=frames to capture
	int nextFrame = strtol(argv[argpos++], &p, 10); //Convert to base 10
	if (*p != '\0')
		fprintf(stderr, "Arg #%d appears to be a non-number!\n", (argpos-1));
	AVPacket packet;
	while (av_read_frame(pFormatCtx, &packet)>=0) {
		if (packet.stream_index==videoStream) {
			int len = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if (len<0) return -1;

			if (frameFinished) {
				frameCount++;
				if (frameCount == videothumb) {
					AVPacket p2;
					p2.size = 0;
					p2.data = NULL;
					av_init_packet(&p2);
					avcodec_encode_video2(trgtCtx, &p2, pFrame, &frameFinished);

					sprintf(thumbnailFilename, "%s/video.jpeg", folder);

					FILE * thumbFile = fopen(thumbnailFilename, "wb");
					if (!thumbFile) {
						printf("Cannot open file to save thumbnail to!(%s)\n", thumbnailFilename);
						return -1;
					}
					fwrite(p2.data, 1, p2.size, thumbFile);
					fclose(thumbFile);
					av_free_packet(&p2);
				}
				if (frameCount == nextFrame) {
					//Encode this frame using the target Encoder and save it to a frame
					pFrame->pts = frameCount;
					pFrame->quality = trgtCtx->global_quality;
					
					AVPacket p2;
					p2.size = 0;
					p2.data = NULL;
					av_init_packet(&p2);
					avcodec_encode_video2(trgtCtx, &p2, pFrame, &frameFinished);

					sprintf(thumbnailFilename, "%s/scene%d.jpeg", folder, (argpos-4-path_override));

					FILE * thumbFile = fopen(thumbnailFilename, "wb");
					if (!thumbFile) {
						printf("Cannot open file to save thumbnail to!(%s)\n", thumbnailFilename);
						return -1;
					}
					fwrite(p2.data, 1, p2.size, thumbFile);
					fclose(thumbFile);
					av_free_packet(&p2);
					if (argc <= argpos) {
						break; //Arguments are exhausted, no need to look for further frames
					}
					nextFrame = strtol(argv[argpos++], &p, 10);
					if (*p != '\0')
						fprintf(stderr, "Arg #%d appears to be a non-number!\n", (argpos-1));
					if (nextFrame < frameCount)
						fprintf(stderr, "Arg #%d specifies a frame that has been passed already. Make sure you pass the frame arguments in ascending numberical order!\n", (argpos-1));
				}
			}

		}
		av_free_packet(&packet);
	}

	av_frame_free(&pFrame);

	avcodec_close(pCodecCtx);
	avcodec_free_context(&trgtCtx);

	avformat_close_input(&pFormatCtx);

	free(videoName);
}
