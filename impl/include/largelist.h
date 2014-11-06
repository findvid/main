#pragma once
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define LLIST_DATA_OFFSET sizeof(LargeList)/sizeof(void *)

#define list_get(l, i) (((void **)l)[LLIST_DATA_OFFSET + (i)])

typedef struct largelist_data_t {
	uint32_t size; //Total size of the list
	uint32_t items; //Current number of items in this block
	uint32_t capacity; //Maximum number of items in this block
	struct largelist_data_t *next;
	//Actual data will be held in what is malloc'd in excess for the first 3 fields!
} LargeList;

typedef struct {
	LargeList * list;
	int index;
} ListIterator;

/**
 * @brief initialize a list data block, each block holding 'size' pointers
 */
LargeList * list_init(uint32_t size);

void list_destroy(LargeList * list);


bool list_isEmpty(LargeList *);

bool list_push(LargeList *, void *);

ListIterator * list_iterate(LargeList *);

void * list_next(ListIterator *);


void list_forall(LargeList *, void (*f)(void *));
