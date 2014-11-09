#include "gtest/gtest.h"

extern "C" {
	#include "readcuts.h"
}

class detectCutsByColors : public testing::Test {
	protected:
	virtual void SetUp() {
		falsealarm = 0;
		hit = 0;
		miss = 0;
	}

	void checkCuts() {
		int vi = 0;
		int di = 0;
		while (vi < video_cuts_count && di < detected_cuts_count) {
			// Detected cut is before the next actual cut
			if (detected_cuts[di] < video_cuts[vi].start) {
				// so an cut has been detected where there is none
				falsealarm++;
				// skip to the next detected one
				di++;
			// Detected cut is after the next actual cut
			} else if (detected_cuts[di] > video_cuts[vi].end) {
				// so a cut has been missed
				miss++;
				// try with the next cut
				vi++;
			// Detected cut is in the next actual cut
			} else {
				// yay a hit
				hit++;
				di++;
				vi++;
			}
		}
		miss += video_cuts_count - v1;
		falsealarm += detected_cuts_count - d1;
	}

	int falsealarm;
	int hit;
	int miss;
	uint32_t *detected_cuts;
	int detected_cuts_count;
	CutInfo *video_cuts;
	int video_cuts_count;
};

TEST_F(detectCutsByColors, bor03) {
	int d_cuts_count = 0;
	uint32_t *d_cuts = NULL;
	int v_cuts_count = 0;
	CutInfo *v_cuts = readCutInfo("bor03.cuts", &v_cuts_count);
}

int main(int argc, char *argv[]) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
