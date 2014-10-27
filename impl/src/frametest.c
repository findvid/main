#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "fvutils.h"
#include "edgedetect.h"

void SaveFrame(AVFrame *pFrame, int width, int height, int i) {
	FILE *pFile;
	char szFilename[32];
	int  y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", i);
	pFile=fopen(szFilename, "wb");
	if(pFile==NULL)
		return;
	
	//Just a macro test for setting and getting pixels in AVFrames/AVPictures
	/* 
	for(y = 0; y < height; y++) {
		setPixel(pFrame,0,y,0xff0000);
		printf("Readback=%x\n", getPixel(pFrame,0,y));
	}*/

	// Print header for .ppm-format
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);   
	// Write bytevectors into file

	//RGB24
	/*
	for(y = 0; y < height; y++)
		fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
	*/

	//GRAY8
	uint8_t lilbuff[3];
	for(y = 0; y < height; y++) {
		for ( int x = 0; x < width; x++ ) {
			lilbuff[0] = pFrame->data[0][x + y*pFrame->linesize[0]];
			lilbuff[1] = pFrame->data[0][x + y*pFrame->linesize[0]];
			lilbuff[2] = pFrame->data[0][x + y*pFrame->linesize[0]];
			fwrite(lilbuff, 1, 3, pFile);
		}
	}

	

	// Close file
	fclose(pFile);
}

int main(int argc, char** argv) {
	// Registers all available codecs
	av_register_all();

	// Struct holding information about container-file
	AVFormatContext * pFormatCtx = NULL;

	if (argc < 2) {
		printf("Missing parameter\n");
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
	// Allocate frame to read conversion into
	//AVFrame *pFrameRGB = av_frame_alloc();

	if (pFrame == NULL) // || pFrameRGB == NULL)
		return -1;
	
	// Some buffer needed for avpicture_fill
	int numBytes = avpicture_get_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	// Initialize pFrameRGB as empty frame
	//avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

	// Object needed to perform conversions from a source dimension to a destination dimension using certain filters
	struct SwsContext *img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
	
	// Finally, start reading packets from the file
	int frameCount = 0;
	int frameFinished = 0;
	AVPacket packet;
	// Mind that we read from pFormatCtx, which is the general container file...
	while (av_read_frame(pFormatCtx, &packet) >= 0 && frameCount <= 0) {
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
				AVFrame * pFrameRGB = getEdgeProfile(pFrame, img_convert_ctx, pCodecCtx->width, pCodecCtx->height);

				// Convert pFrame into a simple bitmap format
				//sws_scale(img_convert_ctx,(const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
				// and save the first 5 frames to disk. Because we can
				SaveFrame((AVFrame *)pFrameRGB, pCodecCtx->width, pCodecCtx->height, frameCount);

				av_free(pFrameRGB);
			}

		}
		av_free_packet(&packet);
	}

	av_free(buffer);
	av_free(pFrame);
	sws_freeContext(img_convert_ctx);
	avcodec_close(pCodecCtx);

	avformat_close_input(&pFormatCtx);
}
