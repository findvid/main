#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include "fvutils.h"
#include "largelist.h"
#include "edgedetect.h"

int main(int argc, char **argv) {
	av_register_all();
	if (argc < 2) return -1;
	VideoIterator * iter = get_VideoIterator(argv[1]);
	//VideoIterator * ref_iter = get_VideoIterator("/home/kanonenfutter/Documents/Wahlprojekt_Videosuche/Testarea/2001/BOR03.mpg");

	if (iter == NULL) return -1;

	AVFrame * frame = av_frame_alloc();


	int gotFrame = 0;
	struct SwsContext *rgb2g_ctx = sws_getContext(iter->cctx->width, iter->cctx->height, iter->cctx->pix_fmt, iter->cctx->width, iter->cctx->height, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
	
	int frames = 0;

	readFrame(iter, frame, &gotFrame);
	while (gotFrame) { 
	
		frame->width = iter->cctx->width;
		frame->height = iter->cctx->height;
		AVFrame * g = getEdgeProfile(frame, rgb2g_ctx);
		SaveFrameG8(g, g->width, g->height, frames);
		frames++;
	
		avpicture_free((AVPicture *)g);
		av_frame_free(&g);
		readFrame(iter, frame, &gotFrame);
	}

	av_frame_free(&frame);
	sws_freeContext(rgb2g_ctx);


	destroy_VideoIterator(iter);
	return 0;
}
