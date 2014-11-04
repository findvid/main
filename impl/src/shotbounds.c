#include "fvutils.h"
#include "edgedetect.h"
#include "histograms.h"

#define DESTINATION_WIDTH 320
#define DESTINATION_HEIGHT 200

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
	AVFrame *pFrameG8 = av_frame_alloc();
	AVFrame *pFrameRGB24 = av_frame_alloc();

	if (pFrame == NULL || pFrameRGB24 == NULL || pFrameG8 == NULL) 
		return -1;

	
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT);
	uint8_t *buffer_rgb24 = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)pFrameRGB24, buffer_rgb24, PIX_FMT_RGB24, DESTINATION_WIDTH, DESTINATION_HEIGHT);

	numBytes = avpicture_get_size(PIX_FMT_GRAY8, DESTINATION_WIDTH, DESTINATION_HEIGHT);
	uint8_t *buffer_g8 = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)pFrameG8, buffer_g8, PIX_FMT_GRAY8, DESTINATION_WIDTH, DESTINATION_HEIGHT);

	// Object needed to perform conversions from a source dimension to a destination dimension using certain filters
	//Convert into a smaller frame for easier processing
	struct SwsContext *convert_rgb24 = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	struct SwsContext *convert_g8 = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, DESTINATION_WIDTH, DESTINATION_HEIGHT, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
	
	// Finally, start reading packets from the file
	int frameCount = 0;
	int frameFinished = 0;
	AVPacket packet;
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
				
				sws_scale(convert_rgb24, pFrame->data, pFrame->linesize, 0, 0, pFrameRGB24->data, pFrameRGB24->linesize);
				sws_scale(convert_g8, pFrame->data, pFrame->linesize, 0, 0, pFrameG8->data, pFrameG8->linesize);

			}

		}
		av_free_packet(&packet);
	}

	av_free(pFrame);
	av_free(pFrameG8);
	av_free(pFrameRGB24);
	sws_freeContext(convert_rgb24);
	sws_freeContext(convert_g8);
	avcodec_close(pCodecCtx);

	avformat_close_input(&pFormatCtx);

}
