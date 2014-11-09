#include "gtest/gtest.h"

extern "C" {
	#include "readcuts.h"
	int processVideo(char *filename, uint32_t **cuts);
}

class detectCutsByColors : public testing::Test {
	protected:
	virtual void SetUp() {
		detected_cuts_count = processVideo("testfiles/BOR03.mpg", &detected_cuts);
		video_cuts = readCutInfo("testfiles/bor03.cuts", &video_cuts_count);
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
		miss += video_cuts_count - vi;
		falsealarm += detected_cuts_count - di;
	}

	void evalResults() {
		printf("Found %d of %d cuts (%2f%)\n", hit, video_cuts_count, (((double)hit)*100.0)/(double)video_cuts_count);
		printf("%d of %d detected were false alarm (%2f%)\n", falsealarm, detected_cuts_count, (((double)falsealarm)*100.0)/(double)detected_cuts_count);
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
	checkCuts();
	evalResults();
}

int main(int argc, char *argv[]) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
