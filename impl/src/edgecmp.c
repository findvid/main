#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mem.h>

#include <signal.h>

#include "fvutils.h"
#include "largelist.h"
#include "edgedetect.h"

uint32_t results[1024];
uint32_t respos;

void sig_handler(int signum) {
	printf("Drawing graph prematurely\n");
	drawGraph(results, respos, 500, 1.0, 1);
	exit(0);
}

int main(int argc, char **argv) {
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	av_register_all();
	VideoIterator * iter = get_VideoIterator("testfiles/hardcuts.mp4");
	VideoIterator * ref_iter = get_VideoIterator("testfiles/hardcuts_edgesffmpeg.mp4");
	//VideoIterator * ref_iter = get_VideoIterator("/home/kanonenfutter/Documents/Wahlprojekt_Videosuche/Testarea/2001/BOR03.mpg");

	if (iter == NULL) return -1;
	if (ref_iter == NULL) return -1;

	AVFrame * frame = av_frame_alloc();
	AVFrame * refedges = av_frame_alloc();
	avpicture_alloc((AVPicture *)refedges, PIX_FMT_GRAY8, iter->cctx->width, iter->cctx->height);
	AVFrame * realedges = av_frame_alloc();


	int gotFrame = 0;
	int gotFrame2 = 0;
	struct SwsContext *rgb2g_ctx = sws_getContext(iter->cctx->width, iter->cctx->height, iter->cctx->pix_fmt, iter->cctx->width, iter->cctx->height, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
	
	struct SwsContext *g2g_ctx = sws_getContext(ref_iter->cctx->width, ref_iter->cctx->height, ref_iter->cctx->pix_fmt, iter->cctx->width, iter->cctx->height, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);

	readFrame(iter, frame, &gotFrame);
	readFrame(ref_iter, realedges, &gotFrame2);
	while (gotFrame && gotFrame2) { 
	
		frame->width = iter->cctx->width;
		frame->height = iter->cctx->height;
		AVFrame * g = getEdgeProfile(frame, rgb2g_ctx);
		SaveFrameG8(g, g->width, g->height, respos);

		//Convert FFMPEG's edgeprofile frame to grayscale
		sws_scale(g2g_ctx, (const uint8_t * const *)realedges->data, realedges->linesize, 0, ref_iter->cctx->height, refedges->data, refedges->linesize);


		g->width = iter->cctx->width;
		g->height = iter->cctx->height;
		refedges->width = iter->cctx->width;
		refedges->height = iter->cctx->height;
		results[respos++] = (uint32_t)(500 * cmpProfiles(g, refedges));	
	
		avpicture_free((AVPicture *)g);
		av_frame_free(&g);
		readFrame(iter, frame, &gotFrame);
		readFrame(ref_iter, realedges, &gotFrame2);
	}

	avpicture_free((AVPicture *)refedges);
	av_frame_free(&refedges);
	avpicture_free((AVPicture *)realedges);
	av_frame_free(&realedges);
	avpicture_free((AVPicture *)frame);
	av_frame_free(&frame);
	sws_freeContext(rgb2g_ctx);
	sws_freeContext(g2g_ctx);

	drawGraph(results, respos, 500, 1.0, 1);

	destroy_VideoIterator(iter);
	destroy_VideoIterator(ref_iter);
	return 0;
}
