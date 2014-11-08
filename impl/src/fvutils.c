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