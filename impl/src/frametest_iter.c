#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "fvutils.h"

//Leak free version
/*
int main(int argc, char **argv) {
	av_register_all();
	if (argc < 2) return -1;
	VideoIterator * iter = get_VideoIterator(argv[1]);

	if (iter == NULL) return -1;
	
	int gf = 0;
	AVFrame * f = nextFrame(iter, &gf);
	av_frame_free(&f);

	AVFrame *pFrameRGB = av_frame_alloc();
	avpicture_alloc((AVPicture *)pFrameRGB, PIX_FMT_RGB24, iter->cctx->width, iter->cctx->height);
	avpicture_free((AVPicture *)pFrameRGB);
	av_frame_free(&pFrameRGB);
	destroy_VideoIterator(iter);
}
*/

//1 free is missing, seemingly the size of a frame
int main(int argc, char **argv) {
	av_register_all();
	if (argc < 2) return -1;
	VideoIterator * iter = get_VideoIterator(argv[1]);

	if (iter == NULL) return -1;

	AVFrame * frame;
	int i = 0;
	int gotFrame;
	
	AVFrame *pFrameRGB = av_frame_alloc();
	avpicture_alloc((AVPicture *)pFrameRGB, PIX_FMT_RGB24, iter->cctx->width, iter->cctx->height);

	struct SwsContext *img_convert_ctx = sws_getContext(iter->cctx->width, iter->cctx->height, iter->cctx->pix_fmt, iter->cctx->width, iter->cctx->height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	while ((frame = nextFrame(iter, &gotFrame)) != NULL) { // != NULL && gotFrame) {
		sws_scale(img_convert_ctx, (const uint8_t * const *)frame->data, frame->linesize, 0, iter->cctx->height, pFrameRGB->data, pFrameRGB->linesize);
		SaveFrameRGB24(pFrameRGB, iter->cctx->width, iter->cctx->height, i++);
		av_frame_free(&frame);
	}
	
	sws_freeContext(img_convert_ctx);
	
	avpicture_free((AVPicture *)pFrameRGB);
	av_frame_free(&pFrameRGB);
	destroy_VideoIterator(iter);
	return 0;
}