#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int i) {
	FILE *pFile;
	char szFilename[32];
	int  y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", i);
	pFile=fopen(szFilename, "wb");
	if(pFile==NULL)
		return;
   


	fprintf(pFile, "P6\n%d %d\n255\n", width, height);   
	for(y = 0; y < height; y++)
		fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
								  
	// Close file
	fclose(pFile);
}

int main(int argc, char** argv) {
	av_register_all();
	AVFormatContext * pFormatCtx = NULL;

	if (argc < 2) {
		printf("Missing parameter\n");
		return -1;
	}

	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
		return -1;
	
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1; // Couldn't find stream information
	
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
		printf("Unsupported Codec\n");
		return -1;
	}
	
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		return -1;
	}

	AVFrame *pFrame = av_frame_alloc();

	AVFrame *pFrameRGB = av_frame_alloc();

	if (pFrame == NULL || pFrameRGB == NULL)
		return -1;

	int numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

	struct SwsContext *img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	
	int i = 0;
	int frameCount = 0;
	int frameFinished = 0;
	AVPacket packet;
	while (av_read_frame(pFormatCtx, &packet)>=0) {
		if (packet.stream_index==videoStream) {
			int len = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if (len<0) return -1;

			if (frameFinished) {
				frameCount++;

				sws_scale(img_convert_ctx,(const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
				if (frameCount<=5)
					SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, frameCount);
			}

		}
		av_free_packet(&packet);
	}

	av_free(buffer);
	av_free(pFrameRGB);
	av_free(pFrame);

	avcodec_close(pCodecCtx);

	avformat_close_input(&pFormatCtx);
}
