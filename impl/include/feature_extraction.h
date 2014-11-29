#pragma once

#include "fvutils.h"
#include "largelist.h"

#define FEATURE_AMNT 4

typedef struct {
	// TODO: Shouldn't this be a double pointer?
	uint32_t *** feature_list; //< Pointer to the features arrays
	uint32_t * feature_length; //< Length of each feature array
	uint32_t feature_count; //< Amount of features in feature_list
} FeatureTuple;

/**
 * Calculates features for a video file and extracts thumbnails.
 * The resulting feature tuple needs to be freed with destroyFeatures().
 *
 * The scene thumbnails are saved in "expath/<videoname>/scene<i>.jpeg"
 * The video thumbnails are saved in "expath/<videoname>/video.jpeg"
 * Where <videoname> is the name of the video file without the file
 * extension and <i> is a upcounting number for each scene starting at 0.
 *
 * @param filename	Path to the video file
 * @param expath	Path where thumbnails should be saved
 * @param vidThumb	Index of the frame that should be taken as video thumbnail
 * @param sceneFrames	Array of indieces of frames that represend each scene
 * @param sceneCount	Amount of scene (length of scenceFrames)
 *
 * @return		On success pointer to a newly allocated FeatureTuple
 *			On failure NULL
 */
FeatureTuple * getFeatures(const char * filename, const char * expath, int vidThumb, uint32_t * sceneFrames, int sceneCount);

/**
 * Frees a FeatureTuple
 *
 * @param t	FeatureTuple to be freed
 */
void destroyFeatures(FeatureTuple * t);
