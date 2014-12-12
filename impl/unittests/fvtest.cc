#include <limits.h>
#include "gtest/gtest.h"


extern "C" {
	#include "fvutils.h"
}

TEST(Framerate, Framerate_Num) {
	EXPECT_EQ(30.0, getFramerate("./testfiles/hardcuts.mp4"));
}


int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

