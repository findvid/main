#include "gtest/gtest.h"

extern "C" {
#include "histograms.h"
}

TEST(GETBINRGB, minimumBin) {
	EXPECT_EQ(0b000000, GETBINRGB(0,0,0));
}

TEST(GETBINRGB, maximumBin) {
	EXPECT_EQ(0b111111, GETBINRGB(255,255,255));
}

TEST(GETBINRGB, maxRed) {
	EXPECT_EQ(0b110000, GETBINRGB(255,0,0));
}

TEST(GETBINRGB, maxGreen) {
	EXPECT_EQ(0b001100, GETBINRGB(0,255,0));
}

TEST(GETBINRGB, maxBlue) {
	EXPECT_EQ(0b000011, GETBINRGB(0,0,255));
}

TEST(GETBINRGB, someBins) {
	EXPECT_EQ(0b010001, GETBINRGB(123,45,67));
	EXPECT_EQ(0b001100, GETBINRGB(1,234,56));
	EXPECT_EQ(0b000000, GETBINRGB(63,63,63));
	EXPECT_EQ(0b010101, GETBINRGB(64,64,64));
}


TEST(GETBINHSV, minimumBin) {
	EXPECT_EQ(0b0000000, GETBINHSV(0,0,0));
}

TEST(GETBINHSV, maximumBin) {
	EXPECT_EQ(0b1111111, GETBINHSV(359,255,255));
}

TEST(GETBINHSV, maxHue) {
	EXPECT_EQ(0b1110000, GETBINHSV(359,0,0));
}

TEST(GETBINHSV, maxSaturation) {
	EXPECT_EQ(0b0001100, GETBINHSV(0,255,0));
}

TEST(GETBINHSV, maxValue) {
	EXPECT_EQ(0b0000011, GETBINHSV(0,0,255));
}

TEST(GETBINHSV, someBins) {
	EXPECT_EQ(0b100001, GETBINHSV(123,45,67));
	EXPECT_EQ(0b001100, GETBINHSV(1,234,56));
	EXPECT_EQ(0b000000, GETBINHSV(44,63,63));
	EXPECT_EQ(0b010101, GETBINHSV(45,64,64));
}

class rgbToHsvTest : public testing::Test {
	protected:

	void HSV_EQ(int h, int s, int v) {
		EXPECT_EQ(hsv.h, h);
		EXPECT_EQ(hsv.s, s);
		EXPECT_EQ(hsv.v, v);
	}

	HSV hsv;
};

TEST_F(rgbToHsvTest, hsvLimits) {
	int r, g, b;
	for (r = 0; r <= 255; r++)
	for (g = 0; g <= 255; g++)
	for (b = 0; b <= 255; b++) {
		rgbToHsv(&hsv, r, g, b);
		EXPECT_TRUE(hsv.h >= 0);
		EXPECT_TRUE(hsv.h <= 359);
		EXPECT_TRUE(hsv.s >= 0);
		EXPECT_TRUE(hsv.s <= 255);
		EXPECT_TRUE(hsv.v >= 0);
		EXPECT_TRUE(hsv.v <= 255);
	}
}

TEST_F(rgbToHsvTest, black) {
	rgbToHsv(&hsv, 0, 0, 0);
	HSV_EQ(0, 0, 0);
}

TEST_F(rgbToHsvTest, white) {
	rgbToHsv(&hsv, 255, 255, 255);
	HSV_EQ(0, 0, 255);
}

TEST_F(rgbToHsvTest, red) {
	rgbToHsv(&hsv, 255, 0, 0);
	HSV_EQ(0, 255, 255);
}

TEST_F(rgbToHsvTest, green) {
	rgbToHsv(&hsv, 0, 255, 0);
	HSV_EQ(120, 255, 255);
}

TEST_F(rgbToHsvTest, blue) {
	rgbToHsv(&hsv, 0, 0, 255);
	HSV_EQ(240, 255, 255);
}

TEST_F(rgbToHsvTest, red2) {
	rgbToHsv(&hsv, 255, 0, 5);
	HSV_EQ(359, 255, 255);
}

TEST_F(rgbToHsvTest, blue2) {
	rgbToHsv(&hsv, 51, 147, 179);
	HSV_EQ(195, 182, 179);
}

class fillHistRgbTest : public testing::Test {
	protected:
	virtual void SetUp() {
		hist = newHistRgb();
		frame = av_frame_alloc();
		frame->width = 600;
		frame->height = 800;		
		avpicture_alloc((AVPicture *)frame, PIX_FMT_RGB24, frame->width, frame->height);
	}

	uint32_t *hist;
	AVFrame *frame;
};

TEST_F(fillHistRgbTest, allBlack) {
	int i;
	fillHistRgb(hist, frame);
	EXPECT_EQ(hist[0], frame->width * frame->height);
	for (i = 1; i < 64; i++) {
		EXPECT_EQ(hist[i], 0);
	}
}

TEST_F(fillHistRgbTest, allWhite) {
	int i;
	for (i = 0; i < 3 * frame->width * frame->height; i++) {
		frame->data[0][i] = 255;
	}
	fillHistRgb(hist, frame);
	EXPECT_EQ(hist[63], frame->width * frame->height);
	for (i = 0; i < 63; i++) {
		EXPECT_EQ(hist[i], 0);
	}
}

TEST_F(fillHistRgbTest, mixedColors) {
	int i;
	// Amount of pixels per color
	int c1 = 200;
	int c2 = 2000;
	int c3 = 79;
	int c1index, c2index, c3index;
	// Set the colors
	for (i = 0; i < c1; i++) {
		frame->data[0][i*3] = 0;
		frame->data[0][i*3+1] = 128;
		frame->data[0][i*3+2] = 51;
		c1index = GETBINRGB(0, 128, 51);
	}
	for (; i < c1 + c2; i++) {
		frame->data[0][i*3] = 240;
		frame->data[0][i*3+1] = 14;
		frame->data[0][i*3+2] = 67;
		c2index = GETBINRGB(240, 14, 67);
	}
	for (; i < c1 + c2 + c3; i++) {
		frame->data[0][i*3] = 170;
		frame->data[0][i*3+1] = 230;
		frame->data[0][i*3+2] = 220;
		c3index = GETBINRGB(170, 230, 220);
	}
	fillHistRgb(hist, frame);
	// Check if the outcome fits the expected amount
	EXPECT_EQ(hist[0], frame->width * frame->height - (c1 + c2 + c3));
	for (i = 1; i < 64; i++) {
		if (i == c1index) EXPECT_EQ(hist[i], c1);
		else if (i == c2index) EXPECT_EQ(hist[i], c2);
		else if (i == c3index) EXPECT_EQ(hist[i], c3);
		else EXPECT_EQ(hist[i], 0);
	}
}


class fillHistHsvTest : public testing::Test {
	protected:
	virtual void SetUp() {
		hist = newHistHsv();
		frame = av_frame_alloc();
		frame->width = 600;
		frame->height = 800;		
		avpicture_alloc((AVPicture *)frame, PIX_FMT_RGB24, frame->width, frame->height);
	}

	uint32_t *hist;
	AVFrame *frame;
};

TEST_F(fillHistHsvTest, allBlack) {
	int i;
	fillHistHsv(hist, frame);
	EXPECT_EQ(hist[0], frame->width * frame->height);
	for (i = 1; i < 128; i++) {
		EXPECT_EQ(hist[i], 0);
	}
}

TEST_F(fillHistHsvTest, allWhite) {
	int i;
	HSV hsv;
	rgbToHsv(&hsv, 255, 255, 255);
	for (i = 0; i < 3*frame->width * frame->height; i++) {
		frame->data[0][i] = 255;
	}
	fillHistHsv(hist, frame);
	for (i = 0; i < 128; i++) {
		if (i == GETBINHSV(hsv.h, hsv.s, hsv.v)) EXPECT_EQ(hist[i], frame->width * frame->height);
		else EXPECT_EQ(hist[i], 0);
	}
}

TEST_F(fillHistHsvTest, mixedColors) {
	int i;
	// Amount of pixels per color
	int c1 = 200;
	int c2 = 2000;
	int c3 = 79;
	int c1index, c2index, c3index;
	HSV hsv;
	// Set the colors
	for (i = 0; i < c1; i++) {
		frame->data[0][i*3] = 0;
		frame->data[0][i*3+1] = 128;
		frame->data[0][i*3+2] = 51;
		rgbToHsv(&hsv, 0, 128, 51);
		c1index = GETBINHSV(hsv.h, hsv.s, hsv.v);
	}
	for (; i < c1 + c2; i++) {
		frame->data[0][i*3] = 240;
		frame->data[0][i*3+1] = 14;
		frame->data[0][i*3+2] = 67;
		rgbToHsv(&hsv, 240, 14, 67);
		c2index = GETBINHSV(hsv.h, hsv.s, hsv.v);
	}
	for (; i < c1 + c2 + c3; i++) {
		frame->data[0][i*3] = 170;
		frame->data[0][i*3+1] = 230;
		frame->data[0][i*3+2] = 220;
		rgbToHsv(&hsv, 170, 240, 220);
		c3index = GETBINHSV(hsv.h, hsv.s, hsv.v);
	}
	fillHistHsv(hist, frame);
	// Check if the outcome fits the expected amount
	EXPECT_EQ(hist[0], frame->width * frame->height - (c1 + c2 + c3));
	for (i = 1; i < 128; i++) {
		if (i == c1index) EXPECT_EQ(hist[i], c1);
		else if (i == c2index) EXPECT_EQ(hist[i], c2);
		else if (i == c3index) EXPECT_EQ(hist[i], c3);
		else EXPECT_EQ(hist[i], 0);
	}
}

TEST(histDiffRgbTest, sameHist) {
	uint32_t* h1 = newHistRgb();
	h1[0] = 17;
	h1[35] = 123;
	h1[63] = 4;
	EXPECT_EQ(0, histDiffRgb(h1, h1));
	free(h1);
}

TEST(histDiffRgbTest, differentHist) {
	uint32_t* h1 = newHistRgb();
	uint32_t* h2 = newHistRgb();
	int sum = 0;
	int i;
	for (i = 0; i < 64; i++) {
		h1[i] = i;
		h2[i] = 2*i;
		sum += i;
	}
	EXPECT_EQ(sum, histDiffRgb(h1, h2));
	free(h1);
	free(h2);
}

TEST(histDiffHsvTest, sameHist) {
	uint32_t* h1 = newHistHsv();
	h1[0] = 17;
	h1[35] = 123;
	h1[127] = 4;
	EXPECT_EQ(0, histDiffHsv(h1, h1));
	free(h1);
}

TEST(histDiffHsvTest, differentHist) {
	uint32_t* h1 = newHistHsv();
	uint32_t* h2 = newHistHsv();
	int sum = 0;
	int i;
	for (i = 0; i < 128; i++) {
		h1[i] = i;
		h2[i] = 2*i;
		sum += i;
	}
	EXPECT_EQ(sum, histDiffHsv(h1, h2));
	free(h1);
	free(h2);
}

int main(int argc, char *argv[]) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
