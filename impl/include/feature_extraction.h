#pragma once

#include "fvutils.h"
#include "largelist.h"

#define FEATURE_AMNT 4

typedef struct {
	uint32_t *** feature_list;
	uint32_t * feature_length;
	uint32_t feature_count;
} FeatureTuple;

FeatureTuple * getFeatures(char *, char*, int, uint32_t *, int);
void destroyFeatures(FeatureTuple * t);
