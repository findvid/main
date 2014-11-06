#include <limits.h>
#include <gtest/gtest.h>

extern "C" {
	#include "largelist.h"
}

TEST(ListTest, InitTest) {
	LargeList * list = list_init(5);
	EXPECT_EQ(0, list->size);
	EXPECT_EQ(0, list->items);
	EXPECT_EQ(5, list->capacity);
	list_destroy(list);
}

TEST(ListTest, PushTest) {
	LargeList * list2 = list_init(3);
	list_push(list2, (void *)13);
	
	list_push(list2, (void *)3);
	
	list_push(list2, (void *)7);

	EXPECT_EQ(list_get(list2,0),(void *)13);
	EXPECT_EQ(list_get(list2,1),(void *)3);
	EXPECT_EQ(list_get(list2,2),(void *)7);
	list_destroy(list2);
}

TEST(ListTest, PushTest2) {
	LargeList * list2 = list_init(2);
	list_push(list2, (void *)13);
	
	list_push(list2, (void *)3);
	
	list_push(list2, (void *)7);

	EXPECT_EQ(list_get(list2, 0),(void *)13);
	EXPECT_EQ(list_get(list2, 1),(void *)3);
	EXPECT_EQ(list_get(list2->next, 0),(void *)7);
	list_destroy(list2);
}

TEST(ListTest, IterTest) {
	LargeList * list2 = list_init(4);
	list_push(list2, (void *)13);
	list_push(list2, (void *)3);
	list_push(list2, (void *)7);

	ListIterator * iter = list_iterate(list2);
	EXPECT_EQ(list_next(iter),(void *)13);
	EXPECT_EQ(list_next(iter),(void *)3);
	EXPECT_EQ(list_next(iter),(void *)7);
	list_destroy(list2);
}


TEST(ListTest, IterTest2) {
	LargeList * list3 = list_init(2);
	list_push(list3, (void *)13);
	list_push(list3, (void *)3);
	list_push(list3, (void *)7);


	ListIterator * iter = list_iterate(list3);
	EXPECT_EQ(list_next(iter),(void *)13);
	EXPECT_EQ(list_next(iter),(void *)3);
	EXPECT_EQ(list_next(iter),(void *)7);
	list_destroy(list3);
}

TEST(ListTest, SizeTest) {
	LargeList * list = list_init(2);
	list_push(list, (void *)13);
	list_push(list, (void *)3);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);

	EXPECT_EQ(15, list->size);	
	list_destroy(list);
}

void foo(void * elm) {
	EXPECT_EQ(elm, (void *)7);
}

TEST(ListTest, ForallTest) {
	LargeList * list = list_init(2);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);
	list_push(list, (void *)7);

	list_forall(list, foo);

	list_destroy(list);
}
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
