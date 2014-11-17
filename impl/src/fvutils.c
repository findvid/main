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
