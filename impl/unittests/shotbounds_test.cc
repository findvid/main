#include "gtest/gtest.h"

extern "C" {
	#include "readcuts.h"
	#include <string.h>
	int processVideo(char *filename, uint32_t **cuts);
}

class detectCutsByColors : public testing::Test {
	protected:
	virtual void SetUp() {
		falsealarm = 0;
		hit = 0;
		miss = 0;
		nothardcut = 0;
	}

	void checkCuts() {
		int vi = 0;
		int di = 0;
		while (vi < video_cuts_count && di < detected_cuts_count) {
			if (video_cuts[vi].type != 'C') {
				nothardcut++;
				vi++;
			// Detected cut is before the next actual cut
			} else if (detected_cuts[di] < video_cuts[vi].start) {
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
		printf("Found %d of %d cuts (%2f%)\n", hit, video_cuts_count - nothardcut, (((double)hit)*100.0)/(double)(video_cuts_count - nothardcut));
		printf("%d of %d detected were false alarm (%2f%)\n", falsealarm, detected_cuts_count, (((double)falsealarm)*100.0)/(double)detected_cuts_count);
	}

	void runForVideo(char *file) {
		char path[100];
		strcpy(path, "testfiles/");
		strcat(path, file);
		strcat(path, ".cuts");
		video_cuts = readCutInfo(path, &video_cuts_count);
		strcpy(path, "testfiles/");
		strcat(path, file);
		strcat(path, ".mpg");
		detected_cuts_count = processVideo(path, &detected_cuts);
		checkCuts();
		evalResults();
		free(detected_cuts);
		free(video_cuts);
	}

	int falsealarm;
	int hit;
	int miss;
	int nothardcut;
	uint32_t *detected_cuts;
	int detected_cuts_count;
	CutInfo *video_cuts;
	int video_cuts_count;
};

TEST_F(detectCutsByColors, BOR03) {
	runForVideo("BOR03");
}

TEST_F(detectCutsByColors, BOR08) {
	runForVideo("BOR08");
}

//TEST_F(detectCutsByColors, BOR10) {
//	runForVideo("BOR10");
//}

TEST_F(detectCutsByColors, BOR12) {
	runForVideo("BOR12");
}

//TEST_F(detectCutsByColors, BOR17) {
//	runForVideo("BOR17");
//}

TEST_F(detectCutsByColors, UGS01) {
	runForVideo("UGS01");
}

TEST_F(detectCutsByColors, UGS04) {
	runForVideo("UGS04");
}

TEST_F(detectCutsByColors, UGS05) {
	runForVideo("UGS05");
}

TEST_F(detectCutsByColors, UGS09) {
	runForVideo("UGS09");
}

int main(int argc, char *argv[]) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
