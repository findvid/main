#pragma once

#include "fvutils.h"
#include "largelist.h"

#define FEATURE_AMNT 3

/*
 * As the tripple pointer can f**k with the brain quite a bit here is
 * an explaination of it:
 *
 * feature_list[t][s][v]
 * where t is the feature type (e.g. histogram, tinyimage, gist, etc.)
 *	 s is the index of the scene to which the feature belongs
 *	 v is the feature vector (e.g. which bin of a histogram)
 *
 * The length of the dimensions is given as follows:
 *  - length of [t] by FEATURE_AMNT
 *  - length of [s] by feature_count
 *  - length of [v] by feature_length[t]
 */
typedef struct {
	uint32_t *** feature_list; //< Pointer to the features arrays
	uint32_t * feature_length; //< Length of each feature array
	uint32_t feature_count; //< Amount of features in feature_list
} FeatureTuple; // FeatureTuples would be a better name

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
FeatureTuple * getFeatures(const char * filename, const char * hashstring, const char * expath, int vidThumb, uint32_t * sceneFrames, int sceneCount);

/**
 * Frees a FeatureTuple
 *
 * @param t	FeatureTuple to be freed
 */
void destroyFeatures(FeatureTuple * t);
