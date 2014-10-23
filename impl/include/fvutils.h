#include <libswscale/swscale.h>
#define setPixel(p,x,y,c) \
	do { \
	p->data[0][x * 3 + y*p->linesize[0]] = ((uint8_t)(c>>16)); \
	p->data[0][x * 3 + y*p->linesize[0] + 1] = ((uint8_t)(c>>8)); \
	p->data[0][x * 3 + y*p->linesize[0] + 2] = (uint8_t)c; \
	} while(0)

#define getPixel(p,x,y) ((p->data[0][x * 3 + y*p->linesize[0]]<<16) + (p->data[0][x * 3 + y*p->linesize[0] + 1]<<8) + p->data[0][x * 3 + y*p->linesize[0] + 2])


AVFrame * copyFrame(AVFrame *pPic, struct SwsContext * ctx, int width, int height) {
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_RGB24, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_RGB24, width, height);
	sws_scale(ctx, (const uint8_t * const*)pPic->data, pPic->linesize, 0, height, res->data, res->linesize);
	return res;
}
