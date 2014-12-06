#include <stdlib.h>
#include "largelist.h"

LargeList * list_init(uint32_t capacity) {
	if(capacity < 1) return NULL;
	LargeList * list = (LargeList *)malloc(sizeof(LargeList) + sizeof(void *) * capacity);
	if (list == NULL) return list;
	
	list->size = 0;
	list->items = 0;
	list->capacity = capacity;
	list->next = NULL;

	return list;
}

void list_destroy(LargeList * list) {
	while (list!=NULL) {
		LargeList * old = list;
		list = list->next;
		free(old);
	}
}

bool list_isEmpty(LargeList * list) {
	return (list->size == 0);
}

bool list_push(LargeList *list, void * elm) {
	list->size++;
	while (list->items == list->capacity) {
		if (list->next == NULL) list->next = list_init(list->capacity);
		//If it's still/again NULL, no allocation could be made
		if (list->next == NULL) return false;
		
		return list_push(list->next, elm);
	}
	((void **)list)[LLIST_DATA_OFFSET + list->items] = elm;
	list->items++;
	return true;
}

void * list_pop(LargeList * list) {
	while (list->size > list->capacity) {
		list->size--;
		list = list->next;
	}
	list->size--;
	list->items--;
	return list_get(list, list->items);
	/*
	if (list->size > list->capacity) {
		list->size--;
		return list_pop(list->next);
	} else {
		list->size--;
		list->items--;
		return list_get(list, list->items);
	}*/
}

ListIterator * list_iterate(LargeList * list) {
	ListIterator * iter = malloc(sizeof(ListIterator));
	iter->list = list;
	iter->index = 0;
	return iter;
}

void * list_next(ListIterator * iter) {
	//Iterate until iter->list is NULL

	//Go to next data block if this one's end has been reached)
	if (iter->index >= iter->list->capacity) {
		iter->list = iter->list->next; //Might set this to NULL
		iter->index = 0;
	}
	//Reached end of the list
	else if (iter->index >= iter->list->items) {
		iter->list = NULL;
	}


	void * res = NULL;
	if (iter->list != NULL)
		res = list_get(iter->list, iter->index++);

	return res;
}

void list_forall(LargeList * list, void (*f)(void *)) {
	ListIterator * iter = list_iterate(list);
	void * elm;
	while ((elm = list_next(iter)) != NULL ) {
		f(elm);
	}
	free(iter);
}
