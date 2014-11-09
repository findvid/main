#include <math.h>
#include "edgedetect.h"

void imageAdd(AVFrame * img1, AVFrame **imgs, int c, int width, int height) {
	for (int x = 0; x < width; x++ ) {
		for ( int y = 0; y < height; y++ ) {
			int g = 0;
			for(int i = 0; i < c; i++)
				g += getPixelG8(imgs[i], x, y);

			setPixelG8(img1, x, y, g);
		}
	}
}

//Improve contrast by linear scaling
void linearScale(AVFrame * pic, int width, int height) {
	int min = 255, max = 0;


	for( int y = 0; y < height; y++ ) {
		for( int x = 0; x < width; x++ ) {
			int p = getPixelG8(pic, x, y);
			if (min > p) min = p;
			else if(max < p) max = p;
		}
	}

	if(min!=0 || max!= 255) {
		double scaling = 255.0 / (max - min);
		for( int y = 0; y < height; y++ ) {
			for( int x = 0; x < width; x++ ) {
				pic->data[0][x + y * pic->linesize[0]] -= min;
				pic->data[0][x + y * pic->linesize[0]] *= scaling;
			}
		}
	}
}

double gauss1(double x, double deviation) {
	return -x/(deviation*deviation) * exp(-(x*x/(2 * deviation * deviation)));
}

double gauss2(double x, double deviation) {
	return x*x/(deviation*deviation*deviation*deviation) * exp(-(x*x/(2 * deviation * deviation)));
}
/**
 * Convolve the area around a given position with a gaussian first derivative operator
 */
double convolveAt(AVFrame * pic, int x, int y, OperatorMask * mask, int width, int height) {
	double output = 0.0;

	for (int p = 0; p < mask->width; p++) {
		//G(-x)*f(x)
		output += getPixelG8(pic, x - mask->off[2 * p], y - mask->off[2 * p + 1]) * mask->weights[p + mask->width];
		//output += getPixelG8(pic, x - mask->off[2 * p], y - mask->off[2 * p + 1]) * gauss1(p, mask->width * 3);
		//G(x)*f(-x)
		output += getPixelG8(pic, x + mask->off[2 * p], y + mask->off[2 * p + 1]) * mask->weights[p];
		//output += getPixelG8(pic, x + mask->off[2 * p], y + mask->off[2 * p + 1]) * gauss1(p, mask->width * 3);
	}

	return fabs(output);
}

AVFrame * getGaussianGradient(AVFrame * gray, OperatorMask * mask, int width, int height) {
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);

	for (int x = 0; x < width; x++)	 {
		for (int y = 0; y < height; y++) {
			setPixelG8(res, x, y, convolveAt(gray, x, y, mask, width, height));
		}
	}

	return res;
}

/**
 * Convolve image with sodel operator
 */
AVFrame * getGradientMagnitudeMap(AVFrame * gray, int width, int height) {
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);


//For each pixel, apply sodel operator to obtain grayscale intensity changes in different directions
	double maxmag = 0.0;
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			
			//Gradients of sodel operator
			int dx = getPixelG8(gray, x-1, y-1) + 2 * getPixelG8(gray, x-1, y) + getPixelG8(gray, x-1, y+1) - getPixelG8(gray, x+1, y-1) - 2 * getPixelG8(gray, x+1, y) - getPixelG8(gray, x+1, y+1);
			int dy = getPixelG8(gray,x-1,y-1) + 2 * getPixelG8(gray, x, y-1) + getPixelG8(gray, x+1, y-1) - getPixelG8(gray, x-1, y+1) - 2 * getPixelG8(gray, x, y+1) - getPixelG8(gray, x+1, y+1);

			//normalize output
			dx /= 9;
			dy /= 9;

			//each pixel saves the magnitude of the sodel operator at this point
 			double mag = sqrt(dx * dx + dy * dy);
			if (maxmag < mag) maxmag = mag;
			res->data[0][x + y * res->linesize[0]] = (uint8_t)mag;
		}
	}

	return res;	
}


/**
 * Takes an array of masks and returns 1 if any mask matches the coord (x,y) as local maximum
 * This assumes there are as many masks as OPERATOR_DIRECTIONS defines
 */
int isEdgePixel(AVFrame * varmap, int x, int y, OperatorMask * masks, int width, int height) {
	int isEdge = 0;
	for ( int i = 0; i < OPERATOR_DIRECTIONS; i++) {
		int isLocalMax = 1;
		int center = getPixelG8(varmap, x, y);
		for (int p = 0; p < masks[i].width; p++) {
			int xo = masks[i].off[2 * p];
			int yo = masks[i].off[2 * p + 1];
			if (getPixelG8(varmap, x-xo, y-yo) >= center) {
				isLocalMax = 0;
			} else if (getPixelG8(varmap, x+xo, y+yo) >= center) {
				isLocalMax = 0;
			}
			if (isLocalMax) isEdge = 1;
		}
	}
	return isEdge;
}

AVFrame * getEdgeProfileSodel(AVFrame *original, struct SwsContext * swsctx, int width, int height) {
	//Retrieve a grayscale copy of the image; the SwsContext must be set properly
	AVFrame *gray = av_frame_alloc();
	avpicture_alloc((AVPicture *)gray, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)gray, buffer, PIX_FMT_GRAY8, width, height);
	sws_scale(swsctx, (const uint8_t * const*)original->data, original->linesize, 0, height, gray->data, gray->linesize);
	
	linearScale(gray, width, height);
	
	AVFrame * sodel = getGradientMagnitudeMap(gray, width, height);
	linearScale(sodel, width, height);

	//SaveFrameG8(sodel, width, height, 20);

	//Initialize an empty frame
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);

	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
		//Mark an edge pixel if it's a significant maximum in the neighborhood
			int center = getPixelG8(sodel, x, y);
			int max_fitness = 0;
			for(int xo = -4; xo <= 4; xo++) {
				for(int yo = -4; yo <= 4; yo++) {
					max_fitness += (center > getPixelG8(sodel, x+xo, y+yo));
				}
			}
			// width = 3 -> 48 pixels (49-1) to compare to
			//75% is smaller than center
			if (max_fitness > 48)
				setPixelG8(res, x, y, 0xff);
		}
	}

	av_free(sodel);
	av_free(gray);
	return res;
}

AVFrame * getEdgeProfileGauss(AVFrame *original, struct SwsContext * swsctx, int width, int height) {
	//Retrieve a grayscale copy of the image; the SwsContext must be set properly
	AVFrame *gray = av_frame_alloc();
	avpicture_alloc((AVPicture *)gray, PIX_FMT_GRAY8, width, height);
	int numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)gray, buffer, PIX_FMT_GRAY8, width, height);
	sws_scale(swsctx, (const uint8_t * const*)original->data, original->linesize, 0, height, gray->data, gray->linesize);

	linearScale(gray, width, height);
	
	//Initialze an empty frame
	AVFrame *res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);
	numBytes = avpicture_get_size(PIX_FMT_GRAY8, width, height);
	buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)res, buffer, PIX_FMT_GRAY8, width, height);


	//Calculate some operator masks
	OperatorMask masks[OPERATOR_DIRECTIONS];
	for ( int a = 0; a < OPERATOR_DIRECTIONS; a++ ) {
		double angle = (3.1415 / OPERATOR_DIRECTIONS) * a;
		masks[a].width = 3;
		masks[a].off = (short *)malloc(sizeof(short) * 2 * 3);
		masks[a].weights = (double *)malloc(sizeof(double) * 2 * 3);
		for (int p = 1; p <= masks[a].width; p++) {
			masks[a].off[2 * (p-1)] = (short)round(p * cos(angle));
			masks[a].off[2 * (p-1) + 1] = (short)round(p * sin(angle));
			masks[a].weights[2 + p] = gauss1(p, 9);
			masks[a].weights[p] = gauss1(-p, 9);
		}
	}
	

	//Other shit
	AVFrame * ggrad1 = getGaussianGradient(gray, &masks[0], width, height);
	AVFrame **ggrads = malloc(5 * sizeof(AVFrame *));
	ggrads[0] = getGaussianGradient(gray, &masks[1], width, height);
	ggrads[1] = getGaussianGradient(gray, &masks[2], width, height);
	ggrads[2] = getGaussianGradient(gray, &masks[3], width, height);
	ggrads[3] = getGaussianGradient(gray, &masks[4], width, height);
	ggrads[4] = getGaussianGradient(gray, &masks[5], width, height);

	imageAdd(ggrad1, ggrads, 5, width, height);

	//linearScale(ggrad1, width, height);
	

	av_free(ggrads[0]);
	av_free(ggrads[1]);
	av_free(ggrads[2]);
	av_free(ggrads[3]);
	av_free(ggrads[4]);
	free(ggrads);

	//Calculate average value for intensity in ggrad1
	double avg = 0.0;
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			avg += (double)(getPixelG8(ggrad1, x, y));
		}
	}
	avg /= (width * height);
	//Mark them as pixel if any mask detects a local extremum in change
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			if (getPixelG8(ggrad1, x, y) > avg)
				res->data[0][x + y * res->linesize[0]] = 255;	
		}
	}

	//Clean up + Ship it
	
	//wreck calculated masks
	for ( int i = 0; i < OPERATOR_DIRECTIONS; i++) {
		free(masks[i].weights);
		free(masks[i].off);
	}

	av_free(ggrad1);
	av_free(gray);	
	
	return res;
}

double edgeDiff(AVFrame * p1, AVFrame * p2, struct SwsContext * ctx, int width, int height) {
	AVFrame * edge1 = getEdgeProfileSodel(p1, ctx, width, height);
	AVFrame * edge2 = getEdgeProfileSodel(p2, ctx, width, height);
	
	int in; // amount of edgepixels in edge1 that are nearby/incident with a pixel in edge2
	int out; // amount of edgepixels in edge2 that are nearby/incident with a pixel in edge1

	int c1; // Total edge pixels in edge1
	int c2; // Total edge pixels in edge2
	
	for ( int x = 0; x < width; x++ ) {
		for ( int y = 0; y < height; y++ ) {
			c1 += (getPixelG8(edge1, x, y)?1:0);
			c2 += (getPixelG8(edge2, x, y)?1:0);

			//increment in/out if the pixel is incident; ideally the edges are thin enough so this wouldn\t work and a dilated edge profile must be used. That's a toDo, however
			in += (getPixelG8(edge1, x, y) && getPixelG8(edge2, x, y) ?1:0);
			//out would currently do the same, so let's not calc it for now
		}
	}

	av_free(edge1);
	av_free(edge2);
	return fmax(1.0 * in / c1, 1.0 * in / c2);
}

void detectCutsByEdges_old(LargeList * list_frames, LargeList * list_cuts, uint32_t startframe, ShotFeedback * feedback, struct SwsContext * swsctx, int width, int height) {
	// Diff values between frames
	double * differences;
	int diff_len;

	int usefeedback; //Encodes wether feedback_diff is NULL or not
	//Check if feedback has been given
	if (feedback->diff != NULL) {
		diff_len = (feedback->diff_len + list_frames->size);
		differences = (double *)malloc(sizeof(double) * diff_len);

		usefeedback = 1;
		
		printf("Receiving feedback worth %d values\n", feedback->diff_len);

		//Copy values
		for (int i = 0; i < feedback->diff_len; i++ ) {
			differences[i] = feedback->diff[i];
		}
		//Only freed once in callee after processing is done
		free(feedback->diff);
		printf("Feedback-Frame@0x%x\n", feedback->lastFrame);
	} else {
		diff_len = (list_frames->size - 1);
		differences = (double *)malloc(sizeof(double) * diff_len);
		usefeedback = 0;
	}

	ListIterator * iter = list_iterate(list_frames);

	//Get edge profiles of all frames in the bulk to avoid calculating each profile twice(except for the first and last one)
	LargeList * list_edge_profiles = list_init(list_frames->capacity);
	if (usefeedback) list_push(list_edge_profiles, getEdgeProfileSodel(feedback->lastFrame, swsctx, width, height));
	for ( int i = 0; i < list_frames->size; i++ ) {
		list_push(list_edge_profiles, getEdgeProfileSodel(list_next(iter), swsctx, width, height));
	}

	int pos = (usefeedback?feedback->diff_len:0);

	//"Reset" iterator
	free(iter);
	iter = list_iterate(list_edge_profiles);

	AVFrame * lastFrame = list_next(iter);
	AVFrame * thisFrame;

	while ( (thisFrame = (AVFrame *)list_next(iter)) != NULL ) {
		//differences[pos++]= edgeDiff(lastFrame, thisFrame, swsctx, width, height);
		int in; // amount of edgepixels in edge1 that are nearby/incident with a pixel in edge2
		int out; // amount of edgepixels in edge2 that are nearby/incident with a pixel in edge1
	
		int c1; // Total edge pixels in edge1
		int c2; // Total edge pixels in edge2
	
		for ( int x = 0; x < width; x++ ) {
			for ( int y = 0; y < height; y++ ) {
				c1 += (getPixelG8(lastFrame, x, y)?1:0);
				c2 += (getPixelG8(thisFrame, x, y)?1:0);

				//increment in/out if the pixel is incident; ideally the edges are thin enough so this wouldn\t work and a dilated edge profile must be used. That's a toDo, however
				in += (getPixelG8(lastFrame, x, y) && getPixelG8(thisFrame, x, y));
				//out would currently do the same, so let's not calc it for now
			}
		}

		differences[pos++] = fmax(1.0 * in / c1, 1.0 * in / c2);
		lastFrame = thisFrame;
	}
	printf("Disposing %d profiles\n", list_edge_profiles->size);
	list_forall(list_edge_profiles, av_free);
	list_destroy(list_edge_profiles);
	free(iter);

	//ToDO/Refine:
	//Search for maxima in differences and mark them as cut
	int vars_count = list_frames->size + (usefeedback?feedback->diff_len:-1);
	double extr = 0.0; //value of last extremum found
	int asc = 1; //Search for a maximum first
	for (int i = 0; i < vars_count; i++) {
		if (asc)
			if (differences[i] > extr) {
				extr = differences[i];
			} else { //Found a a maximum at i-1
				asc = 0;
				list_push(list_cuts, (void *)(startframe + i - (usefeedback?feedback->diff_len:1)));
			}
		else
			if (differences[i] < extr) {
				extr = differences[i];
			} else { //Found a minimum, search for a maximum again
				asc = 1;
			}
	}
	printf("Detected %d cuts within so far\n", list_cuts->size);

	//If there are less than MAX_FEEDBACK_LENGTH frames, the bulk was too small. Just use what we get then, although this likely means the video has ended already and this will be discarded anyway. Possible optimization by initializing an empty array without copying anything into it
	int fb_len = (list_frames->size < MAX_FEEDBACK_LENGTH?list_frames->size:MAX_FEEDBACK_LENGTH);
	
	double * new_feedback = (double *)malloc(sizeof(double) * fb_len);
	feedback->diff_len = fb_len;
	
	printf("New feedback @ 0x%x\n", new_feedback);

	for (int i = 0; i < fb_len ; i++) {
		printf("Get element differences[%d - %d + %d = %d]\n", diff_len, fb_len, i, (diff_len - fb_len +i));
		new_feedback[i] = differences[diff_len - fb_len + i];
	}
	feedback->diff = new_feedback;
	printf("Freeing differences\n");
	free(differences);
	printf("Freed differences\n");
}

void detectCutsByEdges(LargeList * list_frames, LargeList * list_cuts, uint32_t startframe, ShotFeedback * feedback, struct SwsContext * swsctx, int width, int height) {
	//Store the difference values between each frame
	double * differences;
	int diff_len;

	//Encodes wether feedback is being used or not
	int usefeedback;
	if ( feedback->diff != NULL ) {
		//Allocate space for the feedback + n-1 differences and another one for "lastFrame - firstFrame"
		diff_len = (feedback->diff_len + list_frames->size);
		differences = malloc(sizeof(double) * diff_len);
		//Copy over the feedback vars
		for (int i = 0; i < feedback->diff_len; i++ ) {
			differences[i] = feedback->diff[i];
		}
		//
		free(feedback->diff);
		
		usefeedback = 1;
	} else {
		//Without feedback, we calculate n-1 differences, 1 between each frame
		diff_len = (list_frames->size - 1);
		differences = malloc(sizeof(double) * diff_len);
		usefeedback = 0;
	}

	ListIterator * iter = list_iterate(list_frames);
	
	AVFrame * lastFrame;
	if (usefeedback) 
		lastFrame = getEdgeProfileSodel(feedback->lastFrame, swsctx, width, height);
	else
		lastFrame = getEdgeProfileSodel(list_next(iter), swsctx, width, height);

	AVFrame * thisFrame;
	int pos = (usefeedback?feedback->diff_len:0);
	while ((thisFrame = list_next(iter)) != NULL) {
		thisFrame = getEdgeProfileSodel(thisFrame, swsctx, width, height);

		int c1 = 0;
		int c2 = 0;

		int intersects = 0;

		for ( int x = 0; x < width; x++ )
			for ( int y = 0; y < height; y++ ) {
				c1 += (getPixelG8(lastFrame, x, y)?1:0);
				c2 += (getPixelG8(thisFrame, x, y)?1:0);

				intersects += (getPixelG8(lastFrame, x, y) && getPixelG8(thisFrame, x, y));
			}

		differences[pos++] = intersects / fmin(c1, c2);
		printf("differences[%d] = %f\n", (pos-1), differences[(pos-1)]);
		av_free(lastFrame);
		lastFrame = thisFrame;
	}

	av_free(thisFrame);
	
	//Start searching through differences to mark cuts
	//...

	//create and fill new feedback array
	int fb_len = (list_frames->size < MAX_FEEDBACK_LENGTH?list_frames->size:MAX_FEEDBACK_LENGTH);

	double * newdiff = malloc(sizeof(double) * fb_len);
	for (int i = 0; i < fb_len; i++) {
		newdiff[i] = differences[(diff_len - fb_len + i)];
	}
	feedback->diff = newdiff;
	feedback->diff_len = fb_len;
}
