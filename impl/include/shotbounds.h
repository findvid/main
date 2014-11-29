#pragma once

/**
 * Detects cuts in a video.
 *
 * @param filename	Path to the video file
 * @param cuts		Gets filled with a newly allocated
 *			array containing the detected cuts
 *
 * @return		On success the amount of detected cuts
 *			On failure a negative error value
 */
int processVideo(const char *filename, uint32_t **cuts);
