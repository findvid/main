#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>
#include <math.h>


typedef struct {
	uint8_t *data;
	int w;
	int h;
} Image;


int loadImage(Image *img, char *filename) {
	FILE *pFile;
	pFile = fopen(filename, "rb");
	if (pFile == NULL) {
		return -1;
	}
	
	fscanf(pFile, "P6\n%d %d\n255\n", &(img->w), &(img->h));
	img->data = (uint8_t*) av_malloc(3*img->w*img->h*sizeof(uint8_t));
	fread(img->data, 1, 3*img->w*img->h, pFile);
	return 0;
}

int saveImage(Image *img, char *filename) {
	FILE *pFile;
	pFile = fopen(filename, "wb");
	if (pFile == NULL) {
		return -1;
	}
	
	fprintf(pFile, "P6\n%d %d\n255\n", img->w, img->h);
	fwrite(img->data, 1, 3*img->w*img->h, pFile);
	return 0;
}

/*
 * Calculates the position a pixel has in the histogram (4x4x4)
 * 
 * img:	uint8_t*	with rgb color values
 * x:	int		giving the start position for a pixel
 *			the next 3 values of img are the rgb-values of that pixel
 */
#define HISTPOS4(img, x) ((img[x+2] >> 6) + 4 * ((img[x+1] >> 6) + 4 * (img[x] >> 6)))

/*
 * Allocates a 4x4x4 histogram and returns the pointer to it
 * It will not be inizilized with 0
 */
uint32_t* newHist4() {
	return (uint32_t *)av_malloc(sizeof(uint32_t) *4*4*4);
}

/*
 * Calculates a 4x4x4 histogram for img and returns the results in hist
 * hist is an array with 64 elements while it's index is orginized
 * like this: 0bRRGGBB
 * 
 * img:		Image with pixel data and a width/height
 * hist:	array for the calculated histogram
 */
void calcHist4(Image *img, uint32_t *hist) {
	int i;
	for (i = 0; i < 4*4*4; i++) {
		hist[i] = 0;
	}
	for (i = 0; i < img->w*img->h; i++) {
		hist[HISTPOS4(img->data, i*3)] += 1;
	}
}

/*
 * Calculates the difference between two 4x4x4 histograms
 * 
 * h1:	First histogram
 * h2:	Second histogram
 */
uint32_t histDiff(uint32_t *h1, uint32_t *h2) {
	int i;
	int diff = 0;
	for (i = 0; i < 4*4*4; i++) {
		diff += abs(h1[i] - h2[i]);
	}
	return diff;
}

/*
 * Draw a bar on an image
 */
void drawBar(Image *img, int pos, int h) {
	if (h < 0) h = 0;
	if (h >= img->h) h = img->h-1;
	int x = pos;
	int y = 0;
	for (y = 0; y <= h; y++) {
		img->data[3*(((img->h-1)-y)*img->w + x)] = 255;
	}
}

#define MAX(x, y) (x > y) ? x : y
#define MIN(x, y) (x < y) ? x : y

/*
 * Draw a bar on an image
 */
void drawBar2(Image *img, int pos, int h) {
	h += (img->h/2);
	if (h < 0) h = 0;
	if (h >= img->h) h = img->h-1;
	int x = pos;
	int y = MIN(h,img->h/2);
	int y2 = MAX(h, img->h/2);
	for (; y <= y2; y++) {
		img->data[3*(((img->h-1)-y)*img->w + x)] = 255;
	}
}

int main(int argc, char** argv) {
	// Registers all available codecs
	av_register_all();

	// Struct holding information about container-file
	AVFormatContext * pFormatCtx = NULL;

	if (argc < 2) {
		printf("Usage: %s <videofile>\n", argv[0]);
		return -1;
	}

	// Read format information into struct
	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
		return -1;
	
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1; // Couldn't find stream information
	
	// Search for a video stream, assuming there's only one, we pick the first and continue
	int i;
	int videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++ ) {
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
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
	AVFrame *pFrameRGB = av_frame_alloc();

	if (pFrame == NULL || pFrameRGB == NULL)
		return -1;
	

	avpicture_alloc((AVPicture *)pFrameRGB, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	pFrameRGB->width = pCodecCtx->width;
	pFrameRGB->height = pCodecCtx->height;

	// Object needed to perform conversions from a source dimension to a destination dimension using certain filters
	struct SwsContext *img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	
	// Finally, start reading packets from the file
	int frameCount = 0;
	int frameFinished = 0;
	AVPacket packet;

	#define MAX_FRAMES 1024
	#define HEIGHT 200
	uint32_t *last = newHist4();
	uint32_t *this = newHist4();
	uint32_t *diff = (uint32_t *)av_malloc(sizeof(uint32_t) * MAX_FRAMES);
	Image graph;
	graph.data = (uint8_t *)av_malloc(sizeof(uint8_t) * MAX_FRAMES * 3 * HEIGHT);
	graph.w = MAX_FRAMES;
	graph.h = HEIGHT;
	Image graph2;
	graph2.data = (uint8_t *)av_malloc(sizeof(uint8_t) * MAX_FRAMES * 3 * HEIGHT);
	graph2.w = MAX_FRAMES;
	graph2.h = HEIGHT;
	for (i = 0; i < graph.h * graph.w * 3; i++) {
		graph.data[i] = 0;
		graph2.data[i] = 0;
	}

	// Mind that we read from pFormatCtx, which is the general container file...
	while (av_read_frame(pFormatCtx, &packet) >= 0 && frameCount < MAX_FRAMES) {
		// ... therefore, not every packet belongs to our video stream!
		if (packet.stream_index == videoStream) {
			// Packet is part of the video stream previously found
			// therefore use the CodecContext previously retrieved with
			// avcodec_find_decoder2 to read the packet into pFrame
			int len = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if (len < 0) return -1;
			// frameFinished is set be avcodec_decode_video2 accordingly
			if (frameFinished) {
				// Convert pFrame into a simple bitmap format
				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
				
				// and save the first 5 frames to disk. Because we can
				//SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, frameCount);

				// Swap pointers
				uint32_t *tmp = last;
				last = this;
				this = tmp;
				// put the frame into Image struct
				Image img;
				img.data = pFrameRGB->data[0];
				img.h = pCodecCtx->height;
				img.w = pCodecCtx->width;
				// calculate the histogram
				calcHist4(&img, this);
				// calculate the diffrence between this and the last frame
				diff[frameCount] = histDiff(last, this);
				// draw the bar in the graph
				drawBar(&graph, frameCount, diff[frameCount] / 10000);
				if (frameCount > 0) drawBar2(&graph2, frameCount, ((int)diff[frameCount] - (int)diff[frameCount-1]) / 10000);
				frameCount++;
			}

		}
		av_free_packet(&packet);
	}

	saveImage(&graph, "graph.ppm");
	saveImage(&graph2, "graph2.ppm");
	free(graph.data);
	free(graph2.data);
	free(last);
	free(this);
	free(diff);

//	av_free(buffer);
	av_free(pFrameRGB);
	av_free(pFrame);

	avcodec_close(pCodecCtx);

	avformat_close_input(&pFormatCtx);
}

/*int main(int argc, char** argv) {
	if (argc < 3) {
		printf("Usage: %s <infile> <outfile>\n", argv[0]);
		return -1;
	}
	Image img;
	loadImage(&img, argv[1]);
	uint32_t *hist = createHistogram(&img);
	int r,g,b,i;
	int count = 0;
	for (r = 0; r < 4; r++)
	for (g = 0; g < 4; g++)
	for (b = 0; b < 4; b++) {
		printf("%d : [%d][%d][%d] : %u\n", r+2*g+4*b, r, g, b, hist[r+2*(g+2*b)]);
		count += hist[r+2*g+4*b];
		for (i=0; i*100 < hist[r+2*g+4*b]; i += 1) img.data[(r+2*g+4*b)+i*img.w+0] = r << 6;
		for (i=0; i*100 < hist[r+2*g+4*b]; i += 1) img.data[(r+2*g+4*b)+i*img.w+1] = g << 6;
		for (i=0; i*100 < hist[r+2*g+4*b]; i += 1) img.data[(r+2*g+4*b)+i*img.w+2] = b << 6;
	}
	printf("%d\n", count);
	saveImage(&img, argv[2]);
	free(img.data);
}*/
