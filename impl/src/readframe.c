#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "fvutils.h"

int readvideo1(int argc, char** argv) {
	// Struct holding information about container-file
	AVFormatContext * pFormatCtx = NULL;

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

	if (pFrame == NULL) // || pFrameRGB == NULL)
		return -1;
	
	// Some buffer needed for avpicture_fill
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

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
			}

		}
		av_free_packet(&packet);
	}

	av_free(buffer);
	av_free(pFrame);
	avcodec_close(pCodecCtx);
	return frameCount;
}

int decode_frame(AVCodecContext *cCtx, AVFrame *frame, int *frameCount, AVPacket *pkt) {
	int len, frameDone;
	
	len = avcodec_decode_video2(cCtx, frame, &frameDone, pkt);
	if (len < 0) {
		printf("Decodeing frame failed.\n");
		exit(1);//return -1;
	}

	if (frameDone) {
		(*frameCount)++;
		// yay frame done
	}
	
	if (pkt->data) {
		pkt->size -= len;
		pkt->data += len; // ?? What's that good for?
	}
	
	return 0;
}

#define BUFFER_SIZE 4096
int readvideo2(int argc, char** argv) {
	AVCodec *codec;
	AVCodecContext *cCtx = NULL;
	AVFormatContext *fCtx = NULL;
	int frameCount = 0;
	FILE *f;
	AVFrame *frame;
	AVPacket pkt;
	uint8_t buf[BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];

	av_init_packet(&pkt);

	// set end of buffer to 0 to prevent overreading for damaged streams
	memset(buf + BUFFER_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);

	// Read format information into struct
	if(avformat_open_input(&fCtx, argv[1], NULL, NULL) != 0)
		return -1;
	
	// Retrieve stream information
	if(avformat_find_stream_info(fCtx, NULL)<0)
		return -1; // Couldn't find stream information
	
	// Search for a video stream, assuming there's only one, we pick the first and continue
	int videoStream = -1;
	for (int i = 0; i < fCtx->nb_streams; i++ )
		if (fCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	
	// Could not find a video stream
	if (videoStream == -1)
		return -1;

	// Retrieve CodecContext
	cCtx = fCtx->streams[videoStream]->codec;
	
	// Get the appropiate Decoder for the video stream
	codec = avcodec_find_decoder(cCtx->codec_id);
	if (!codec) {
		printf("Unsupported Codec\n");
		return -1;
	}
	
	// don't send compleate frames
	cCtx->flags |= CODEC_FLAG_TRUNCATED;
	
	if (avcodec_open2(cCtx, codec, NULL) < 0) {
		return -1;
	}
	
	f = fopen(argv[1], "rb");
	if (!f) {
		printf("Could not open file.\n");
		return -1;
	}

	frame = av_frame_alloc();
	if (!frame) {
		return -1;
	}
	
	for (;;) {
		pkt.size = fread(buf, 1, BUFFER_SIZE, f);
		if (pkt.size == 0) {
			break;
		}
		pkt.data = buf;
		while (pkt.size > 0) {
			decode_frame(cCtx, frame, &frameCount, &pkt);
		}
	}
	
	// Some codecs (e.g. MPEG) need this for the last frame
	pkt.data = NULL;
	pkt.size = 0;
	decode_frame(cCtx, frame, &frameCount, &pkt);
	fclose(f);
	av_frame_free(&frame);
	
	return frameCount;
}

int main(int argc, char** argv) {
	// Registers all available codecs
	av_register_all();

	if (argc < 2) {
		printf("Missing parameter\n");
		return -1;
	}
	
	int f1, f2;
	printf("Start\n");
	f1 = readvideo1(argc, argv);
	printf("F1: %d\n", f1);
	f2 = readvideo2(argc, argv);
	printf("F2: %d\n", f2);
}
