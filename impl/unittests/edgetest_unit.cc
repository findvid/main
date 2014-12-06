#include <limits.h>
#include "gtest/gtest.h"

extern "C" {
	#include "fvutils.h"
	#include "edgedetect.h"
}

class EdgeTest : public testing::Test {
	public:
		AVFrame * img;
		AVFrame * testFrame1(int,int);
		
		uint8_t getImagePixel(int x, int y) {
			int width = 640, height = 400;
			return getPixelG8(img, x, y);
		}

	protected:
		virtual void SetUp() {
			int width = 640, height = 400;
			AVFrame * i = testFrame1(width, height);
			struct SwsContext * sws = sws_getContext(width, height, PIX_FMT_RGB24, width, height, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
			this->img = getEdgeProfile(i, sws, width, height);
			free(i);
		}

	
};

AVFrame * EdgeTest::testFrame1(int width, int height) {
	AVFrame *img = av_frame_alloc();
	avpicture_alloc((AVPicture *)img, PIX_FMT_RGB24, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, width, height);

	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)img, buffer, PIX_FMT_RGB24, width, height);

	int border1 = width/2;
	for ( int xt = 0; xt < width; xt++) {
		for ( int yt = 0; yt < height; yt++) {
			if(xt < border1) {
				setPixel(img, xt, yt, 0xff0000);
			} else
				setPixel(img, xt, yt, 0x00ffff);
		}
	}

	return img;
}

TEST_F(EdgeTest, SimpleEdge1) {
	EXPECT_EQ(255, getImagePixel(320, 50));
	EXPECT_EQ(0, getImagePixel(100, 100));
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
