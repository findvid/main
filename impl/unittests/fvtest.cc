#include <limits.h>
#include "gtest/gtest.h"


extern "C" {
	#include "fvutils.h"
}

TEST(VideoIterator, OneThread) {
	av_register_all();
	VideoIterator * iter = get_VideoIterator("./testfiles/hardcuts.mp4");
	AVFrame * frame = av_frame_alloc();

	int gotFrame = 0;
	printf("?\n");
	readFrame(iter, frame, &gotFrame);
	printf("!\n");
	while (gotFrame) {
		printf("Read frame %lu\n", frame->pkt_dts);
		readFrame(iter, frame, &gotFrame);
	}

	av_frame_free(&frame);
	destroy_VideoIterator(iter);
}

TEST(Framerate, Framerate_Num) {
	EXPECT_EQ(30.0, getFramerate("./testfiles/hardcuts.mp4"));
}


int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

