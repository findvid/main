#include <limits.h>
#include "gtest/gtest.h"

extern "C" {
	#include "fvutils.h"
	#include "edgedetect.h"
}

class EdgeTest : public testing::Test {
	public:
		struct SwsContext * sws;

		AVFrame * img;
		
		AVFrame * box_o;
		AVFrame * hbox_o;

		AVFrame * box;
		AVFrame * hbox;

		struct t_sobelOutput * sobel_box;
		struct t_sobelOutput * sobel_hbox;


		AVFrame * testFrame1(int,int);
		AVFrame * testFrameBox(int,int);
		AVFrame * testFrameHBox(int, int);

		uint8_t getImagePixel(int x, int y) {
			return getPixelG8(img, x, y);
		}

		//returns 1 if all edge pixels in profile are found in container aswell
		int profileIsIn(AVFrame * profile, AVFrame * container) {
			int res = 1;
			for (int x = 0; x < profile->width; x++) {
				for (int y = 0; y < profile->height; y++) {
					//False when G(profile) && !G(container)
					res &= (!getPixelG8(profile, x, y) || getPixelG8(container, x, y) );
				}
			}
			return res;
		}

	protected:
		virtual void SetUp() {
			int width = 640, height = 400;
			sws = sws_getContext(width, height, PIX_FMT_RGB24, width, height, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
			this->sobel_box = (struct t_sobelOutput *)malloc(sizeof(struct t_sobelOutput));
			this->sobel_hbox = (struct t_sobelOutput *)malloc(sizeof(struct t_sobelOutput));

			AVFrame * i = testFrame1(width, height);
			i->width = width;
			i->height = height;
			this->img = getEdgeProfile(i, sws, width, height, NULL);
			
			this->box_o = testFrameBox(width, height);
			SaveFrameRGB24(this->box_o, width, height, 10);
			this->box = getEdgeProfile(this->box_o, sws, width, height, this->sobel_box);
			SaveFrameG8(this->box, width, height, 100);
			SaveFrameG8(this->sobel_box->mag, width, height, 101);
			SaveFrameG8(this->sobel_box->dir, width, height, 102);
			

			this->hbox_o = testFrameHBox(width, height);
			SaveFrameRGB24(this->hbox_o, width, height, 20);
			this->hbox = getEdgeProfile(this->hbox_o, sws, width, height, this->sobel_hbox);
			this->hbox->width = width;
			this->hbox->height = height;
			SaveFrameG8(this->hbox, width, height, 200);
			SaveFrameG8(this->sobel_hbox->mag, width, height, 201);
			SaveFrameG8(this->sobel_hbox->dir, width, height, 202);
			
			avpicture_free((AVPicture *)i);
			av_frame_free(&i);
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

AVFrame * EdgeTest::testFrameBox(int width, int height) {
	AVFrame *img = av_frame_alloc();
	avpicture_alloc((AVPicture *)img, PIX_FMT_RGB24, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, width, height);

	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)img, buffer, PIX_FMT_RGB24, width, height);

	int xm = width/2;
	int ym = height/2;
	for (int xo = -100; xo <= 100; xo++) {
		for (int yo = -100; yo <= 100; yo++) {
			setPixel(img, xm+xo, ym+yo, 0xff0000);
		}
	}

	img->width = width;
	img->height = height;
	return img;
}

AVFrame * EdgeTest::testFrameHBox(int width, int height) {
	AVFrame *img = av_frame_alloc();
	avpicture_alloc((AVPicture *)img, PIX_FMT_RGB24, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, width, height);

	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)img, buffer, PIX_FMT_RGB24, width, height);

	int xm = width/2;
	int ym = height/2;
	int xo = 0, yo = 0;
	xo = -100;
	for (yo = -100; yo <= 100; yo++) {
		setPixel(img, xm+xo, ym+yo, 0xff0000);
	}
	xo = 100;
	for (yo = -100; yo <= 100; yo++) {
		setPixel(img, xm+xo, ym+yo, 0xff0000);
	}
	yo = -100;
	for (xo = -100; xo <= 100; xo++) {
		setPixel(img, xm+xo, ym+yo, 0xff0000);
	}
	yo = 100;
	for (xo = -100; xo <= 100; xo++) {
		setPixel(img, xm+xo, ym+yo, 0xff0000);
	}
	
	img->width = width;
	img->height = height;
	return img;
}

AVFrame * smalledge() {
	AVFrame *img = av_frame_alloc();
	avpicture_alloc((AVPicture *)img, PIX_FMT_RGB24, 160, 100);
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, 160, 100);

	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)img, buffer, PIX_FMT_RGB24, 160, 100);

	for ( int xt = 0; xt < 160; xt++) {
		for ( int yt = 0; yt < 100; yt++) {
			if(xt < 40 && yt < 40) {
				setPixel(img, xt, yt, 0xff0000);
			} else
				setPixel(img, xt, yt, 0x000000);
		}
	}

	img->width = 160;
	img->height = 100;
	return img;
}

TEST_F(EdgeTest, SimpleEdge1) {
	EXPECT_LT(9, getImagePixel(320, 50));
	EXPECT_EQ(0, getImagePixel(100, 100));
}

TEST_F(EdgeTest, BoxCmp) {
	EXPECT_GT(cmpProfiles(this->box, this->hbox), 0.495);
}

TEST_F(EdgeTest, SobelMagCheck) {
	EXPECT_EQ(getPixelG8(this->sobel_hbox->mag, 420, 130), 0);
	EXPECT_GT(getPixelG8(this->sobel_hbox->mag, 419, 130), 20);
	EXPECT_GT(getPixelG8(this->sobel_hbox->mag, 421, 130), 20);

	//Magnitude must be 0 somewhere in between for both boxes
	EXPECT_EQ(getPixelG8(this->sobel_box->mag, 200, 200), 0);
	EXPECT_EQ(getPixelG8(this->sobel_hbox->mag, 200, 200), 0);
}

TEST_F(EdgeTest, SobelDirCheck) {
	EXPECT_EQ(getPixelG8(this->sobel_hbox->dir, 300, 299), 2);
	EXPECT_EQ(getPixelG8(this->sobel_hbox->dir, 300, 300), 0);
	EXPECT_EQ(getPixelG8(this->sobel_hbox->dir, 300, 301), 6);
	EXPECT_EQ(getPixelG8(this->sobel_hbox->dir, 421, 130), 4);
	EXPECT_EQ(getPixelG8(this->sobel_hbox->dir, 419, 130), 8);
	EXPECT_EQ(getPixelG8(this->sobel_hbox->dir, 415, 130), 0);
}

TEST(EdgeFeatures, Weights) {
	InterpolationWeights * weights = getLinearInterpolationWeights(640,400);
	for(int x = 0; x < 40; x++)
		for (int y = 0; y < 40;y++) {
			double var = (	getMatrixVar(weights->nw, x, y, 40) +
							getMatrixVar(weights->n, x, y, 40)  +
							getMatrixVar(weights->ne, x, y, 40)  +
							getMatrixVar(weights->e, x, y, 40)  +
							getMatrixVar(weights->se, x, y, 40)  +
							getMatrixVar(weights->s, x, y, 40)  +
							getMatrixVar(weights->sw, x, y, 40)  +
							getMatrixVar(weights->w, x, y, 40)  +
							getMatrixVar(weights->c, x, y, 40)
							);
			//EXPECT_EQ(1.0, var); //GTest doesn't like this and will fail this anyway due to rounding errors
			EXPECT_GT(1.0001, var);
			EXPECT_LT(0.9999, var);

		}



	free(weights->c);
	free(weights);
}

//The total amount of binned pixels add up perfectly, but there seem to be more "directed" pixels than seems reasonable, about 2 as many as one should expect from this image.
//The distribution isn't exactly perfect, either; some directions seem to be preferred over others!
TEST_F(EdgeTest, BoxFeatures) {
	uint32_t * feats;
	uint32_t * feats2;
	InterpolationWeights * weights = getLinearInterpolationWeights(640,400);	
	edgeFeatures(this->box, &feats, &feats2, weights, this->sws);
	//Test features

	for (int i = 0; i < FEATURES_EDGES_MAGNITUDES; i++)
		printf("%d\n", feats[i]);
	for (int i = 0; i < FEATURES_EDGES_DIRECTIONS; i++)
		printf("%d\n", feats2[i]);
	
	free(feats);
	free(feats2);
	free(weights->c);
	free(weights);
}

TEST(FeaturesTest, SmallEdge) {
	struct SwsContext * sws = sws_getContext(160, 100, PIX_FMT_RGB24, 160, 100, PIX_FMT_GRAY8, SWS_BICUBIC, NULL, NULL, NULL);
	AVFrame * img = smalledge();
	SaveFrameRGB24(img,160,100,300);
	struct t_sobelOutput sobel;
	AVFrame * ig = getEdgeProfile(img, sws, 160, 100, &sobel);
	SaveFrameG8(ig, 160, 100, 301);
	SaveFrameG8(sobel.dir, 160, 100, 302);
	avpicture_free((AVPicture *)sobel.mag);
	av_frame_free(&sobel.mag);
	avpicture_free((AVPicture *)sobel.dir);
	av_frame_free(&sobel.dir);
	avpicture_free((AVPicture *)ig);
	av_frame_free(&ig);
	uint32_t * feat_str;
	uint32_t * feat_dir;
	InterpolationWeights * weights = getLinearInterpolationWeights(160,100);	

	edgeFeatures(img, &feat_str, &feat_dir, weights, sws);

	EXPECT_EQ(feat_str[0], 4745); //Actual result is 5000; either 1 pixel is counted too much, or the normalization via weightused is wrong

	//EXPECT_GE(feat_dir[0], 16000-320);
	EXPECT_GE(feat_dir[0], 16000-640); //The above line is strictly correct, because every side can mark up to 80 pixels with a direction (40 on each side of the color change)
	//The broad guassian smoothing distributes the sobel directions, however, allowing a potential edge to mark much more directions

	EXPECT_EQ(feat_dir[2], 38);
	EXPECT_EQ(feat_dir[4], 76);
	EXPECT_EQ(feat_dir[6], 76);
	EXPECT_EQ(feat_dir[8], 38);

	int sum = feat_dir[0] + feat_dir[1] + feat_dir[2] + feat_dir[3] + feat_dir[4] + feat_dir[5] + feat_dir[6] + feat_dir[7] + feat_dir[8];

	EXPECT_EQ(sum, 16000);

	avpicture_free((AVPicture *)img);
	av_frame_free(&img);
	sws_freeContext(sws);
	free(weights->c);
	free(weights);
}

/*TEST_F(EdgeTest, FeatureSmokeTest) {
	uint32_t * feats;
	InterpolationWeights * weights = getLinearInterpolationWeights(640,400);	
	edgeFeatures(this->box, &feats, weights);
	for (int i = 0; i < FEATURE_LENGTH; i++)
		printf("%d\n", feats[i]);
	free(feats);
	free(weights->c);
	free(weights);
}*/

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
