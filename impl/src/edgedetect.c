#include <math.h>
#include "fvutils.h"
#include "largelist.h"
#include "edgedetect.h"

void saveGraph(int id, double * vars, int vars_len){
printf("SAVE GRAPH %d\n", id);
	/*AVFrame * graph = av_frame_alloc();
	avpicture_alloc((AVPicture *)graph, PIX_FMT_RGB24, vars_len, 200);
	
	for(int i = 0; i < vars_len; i++) {
		int var = (int)(200.0 - vars[i] * 200.0);
		printf("Save %f to g(%d)\n", vars[i], i);
		for (int j = 200; j >= var; j--) {
			setPixel(graph, i, j, 0xff0000);
		}
		for (int j = 0; j < var; j++) {
			setPixel(graph, i, j, 0x0);
		}
	}

	SaveFrameRGB24(graph, vars_len, 200, id);
	avpicture_free((AVPicture *)graph);
	av_frame_free(&graph); */
	uint32_t * data = malloc(sizeof(uint32_t) * vars_len);
	for (int i = 0; i < vars_len; i++) {
		data[i] = vars[i] * 500;
	}
	drawGraph(data, vars_len, 500, 1, id);
	free(data);
}

//EPR = Edge Persist Ratio
//Calculate EPR between two frames
double cmpProfiles(AVFrame * p1, AVFrame * p2) {
	int matches = 0;
	int p1_edges = 0;
	int p2_edges = 0;
	for (int x = 0; x < p1->width; x++) {
		for (int y = 0; y < p1->height; y++) {
			p1_edges += (getPixelG8(p1, x, y)?1:0);
			p2_edges += (getPixelG8(p2, x, y)?1:0);
			matches += (getPixelG8(p1, x, y) && getPixelG8(p2, x, y));
		}
	}
	double res = fmin(((double)matches/p1_edges),((double)matches/p2_edges));
	return res;
}

double EdgeChangeRatio(AVFrame * p1, AVFrame * p2) {
	int c1 = 0;
	int c2 = 0;

	int out = 0;
	int in = 0;
	for ( int x = 0; x < p2->width; x++ )
		for ( int y = 0; y < p2->height; y++ ) {
			c1 += (getPixelG8(p1, x, y)?1:0);
			c2 += (getPixelG8(p2, x, y)?1:0);

			out += (getPixelG8(p1, x, y) && !getPixelG8(p2, x, y));
			in += (!getPixelG8(p1, x, y) && getPixelG8(p2, x, y));
		}

	if (c1 || c2)
		return fmax(1.0 * out / c1, 1.0 *  in / c2);
	else 
		return 0.0;
}

double EdgeContrastLienhart(AVFrame * p) {
	double s = 0.0;
	double w = 0.0;

	for (int x = 0; x < p->width; x++) {
		for (int y = 0; y < p->height; y++) {
			if (getPixelG8(p, x, y) >= 125)
				s += getPixelG8(p, x, y);
			else if (getPixelG8(p, x, y) > 25)
				w += getPixelG8(p, x, y);
		}
	}

	return 1 + ((s - w - 1)/(s + w + 1));
}

//Average edge strength ratio
double EdgeContrast(AVFrame * p) {
	int str = 0.0;
	int amt = 0.0;

	for (int x = 0; x < p->width; x++) {
		for (int y = 0; y < p->height; y++) {
			str += getPixelG8(p, x, y);
			amt += (getPixelG8(p, x, y)?1:0);
		}
	}
	return ((double)str/amt)/255;
}

//Normalized gaussian with µ = 0
double gauss(double x, double deviation) {
	return ((1/(deviation * sqrt(2 * M_PI))) * exp(-(x*x)/(2 * deviation * deviation)));
}

/*
 * creates a mask operator without creating offset values
*/
OperatorMask * getBellOperatorLinear(int width) {
	OperatorMask * mask = malloc(sizeof(OperatorMask));
	mask->width = width;
	mask->weights = malloc(sizeof(double) * (2 * width - 1));
	for (int i = 0; i < width; i++) {
		mask->weights[i] = gauss((double)i, (width/3.0));
	//	printf("Weight[%d] = %f\n", i, mask->weights[i]);
	}
	for (int i = width; i < (2 * width - 1); i++) {
		// (width - 1) - (2 * width - 2) = width - 1 - 2 * width + 2
		// = -width + 1
		int x = (width - 1) - i;
		mask->weights[i] = gauss((double)x, (width/3.0));
	//	printf("Weight[%d] = %f\n", x, mask->weights[i]);
	}
	return mask;
}

//Improve contrast by linear scaling
void linearScale(AVFrame * pic) {
	int min = 255, max = 0;


	for( int y = 0; y < pic->height; y++ ) {
		for( int x = 0; x < pic->width; x++ ) {
			int p = getPixelG8(pic, x, y);
			if (min > p) min = p;
			else if(max < p) max = p;
		}
	}

	if(min!=0 || max!= 255) {
		double scaling = 255.0 / (max - min);
		for( int y = 0; y < pic->height; y++ ) {
			for( int x = 0; x < pic->width; x++ ) {
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

AVFrame * smoothGauss(AVFrame * gray, struct SwsContext * ctx) {
	AVFrame * smoothed = av_frame_alloc();
	avpicture_alloc((AVPicture *)smoothed, PIX_FMT_GRAY8, gray->width, gray->height);
	smoothed->width = gray->width;
	smoothed->height = gray->height;
	/*
	for (int x = 0; x < gray->width; x++) {
		for (int y = 0; y < gray->height; y++) {
			double c = 0.0;
			c += 0.0425 * getPixelG8(gray, x-1, y-1);
			c += 0.0825 * getPixelG8(gray, x  , y-1);
			c += 0.0425 * getPixelG8(gray, x+1, y-1);
			c += 0.0825 * getPixelG8(gray, x-1, y  );
			c += 0.5    * getPixelG8(gray, x  , y  );
			c += 0.0825 * getPixelG8(gray, x+1, y  );
			c += 0.0425 * getPixelG8(gray, x+1, y+1);
			c += 0.0825 * getPixelG8(gray, x  , y+1);
			c += 0.0425 * getPixelG8(gray, x-1, y+1);
			setPixelG8(smoothed, x, y, c);
		}
	} */
	/* TOO BIG! Makes the image washed out and creates multiple edges that sneak past the NMS!! */
	for (int x = 0; x < gray->width; x++) {
		for (int y = 0; y < gray->height; y++) {
			double c = 0.0;
			c += 0.00366 * getPixelG8(gray, x-2, y-2);
			c += 0.01465 * getPixelG8(gray, x-1, y-2);
			c += 0.02564 * getPixelG8(gray, x, y-2);
			c += 0.01465 * getPixelG8(gray, x+1, y-2);
			c += 0.00366 * getPixelG8(gray, x+2, y-2);

			c += 0.01465 * getPixelG8(gray, x-2, y-1);
			c += 0.05860 * getPixelG8(gray, x-1, y-1);
			c += 0.09523 * getPixelG8(gray, x, y-1);
			c += 0.05860 * getPixelG8(gray, x+1, y-1);
			c += 0.01465 * getPixelG8(gray, x+2, y-1);

			c += 0.02564 * getPixelG8(gray, x-2, y);
			c += 0.09523 * getPixelG8(gray, x-1, y);
			c += 0.15018 * getPixelG8(gray, x, y);
			c += 0.09523 * getPixelG8(gray, x+1, y);
			c += 0.02564 * getPixelG8(gray, x+2, y);
	
			c += 0.01465 * getPixelG8(gray, x-2, y+1);
			c += 0.05860 * getPixelG8(gray, x-1, y+1);
			c += 0.09523 * getPixelG8(gray, x, y+1);
			c += 0.05860 * getPixelG8(gray, x+1, y+1);
			c += 0.01465 * getPixelG8(gray, x+2, y+1);
		
			c += 0.00366 * getPixelG8(gray, x-2, y+2);
			c += 0.01465 * getPixelG8(gray, x-1, y+2);
			c += 0.02564 * getPixelG8(gray, x, y+2);
			c += 0.01465 * getPixelG8(gray, x+1, y+2);
			c += 0.00366 * getPixelG8(gray, x+2, y+2);

			setPixelG8(smoothed, x, y, c);
		}
	}
	/**/
	return smoothed;
}

/**
 * Convolve image with sodel operator
 */
void getSobelOutput(AVFrame * gray, struct t_sobelOutput * out) {
	AVFrame *mag = av_frame_alloc();
	avpicture_alloc((AVPicture *)mag, PIX_FMT_GRAY8, gray->width, gray->height);
	mag->width = gray->width;
	mag->height = gray->height;
	AVFrame *dir = av_frame_alloc();
	avpicture_alloc((AVPicture *)dir, PIX_FMT_GRAY8, gray->width, gray->height);
	dir->width = gray->width;
	dir->height = gray->height;


//For each pixel, apply sodel operator to obtain grayscale intensity changes in different directions
	for ( int y = 0; y < gray->height; y++ ) {
		for ( int x = 0; x < gray->width; x++ ) {
			
			//Gradients of sodel operator
			int dx = getPixelG8( gray, x+1, y-1)
				+ 2 * getPixelG8(gray, x+1, y  )
				+ getPixelG8(    gray, x+1, y+1)
				
				- getPixelG8(    gray, x-1, y-1)
				- 2 * getPixelG8(gray, x-1, y  )
				- getPixelG8(    gray, x-1, y+1);

			int dy = getPixelG8( gray, x-1, y+1)
				+ 2 * getPixelG8(gray, x,   y+1)
				+ getPixelG8(    gray, x+1, y+1)
				- getPixelG8(    gray, x-1, y-1)
				- 2 * getPixelG8(gray, x,   y-1)
				- getPixelG8(    gray, x+1, y-1);

			//normalize output
			dx /= 9;
			dy /= 9;

			//each pixel saves the magnitude of the sodel operator at this point
 			double magvar = sqrt(dx * dx + dy * dy);
			setPixelG8(mag, x, y, magvar);

			double dirvar = atan2(dx, dy);
			
			//Encode direction into a gray value: [-90|90] -> [0|4]
			//-180 = 0
			if (dirvar < 0) //]-90|0[ -> ]90|180]
				dirvar = M_PI + dirvar;
			//No need for drastic precision, use multiples of 45 degrees
			//if (dirvar < 0 || dirvar > (M_PI))
			//	printf("invalid dirvar = %f\n", dirvar);

			dirvar = round(dirvar / (M_PI / 4));
		
			dirvar *= (M_PI / 4);

			setPixelG8(dir, x, y, dirvar);
		}
	}

	out->mag = mag;
	out->dir = dir;
}

/*	BENCHMARK
	Takes about 60ms for each frame at 400x300 pixels, scaled down from 1920x1080 pixels
*/

AVFrame * getEdgeProfile(AVFrame * original, struct SwsContext * ctx, int width, int height) {
	// Step 0: Get grayscale picture
	AVFrame *gray = av_frame_alloc();
	avpicture_alloc((AVPicture *)gray,PIX_FMT_GRAY8, width, height);

	sws_scale(ctx, (const uint8_t * const*)original->data, original->linesize, 0, original->height, gray->data, gray->linesize);
	gray->height = height;
	gray->width = width;

	//This seems to easily mess up the sobel operator in the detection of primitive forms	//Step 1: gaussian smoothing
	AVFrame * sgray = smoothGauss(gray, ctx);
	avpicture_free((AVPicture *)gray);
	av_frame_free(&gray);

	//	SaveFrameG8(sgray, sgray->width, sgray->height, 40);

	//Step 2: Get SobelOutput
	struct t_sobelOutput sobel;
	getSobelOutput(sgray, &sobel);
	avpicture_free((AVPicture *)sgray);
	av_frame_free(&sgray);
	
	//SaveFrameG8(sobel.mag, sobel.mag->width, sobel.mag->height, 3);

	linearScale(sobel.mag);
	//	SaveFrameG8(sobel.mag, sobel.mag->width, sobel.mag->height, 40);

	AVFrame * newmag = av_frame_alloc();
	avpicture_alloc((AVPicture *)newmag, PIX_FMT_GRAY8, sobel.mag->width, sobel.mag->height);
	newmag->width = sobel.mag->width;
	newmag->height = sobel.mag->height;

	//Step 3: Non-maxmimum suppression
	//Check for greater magnitudes orthogonal to the edge
	for (int x = 0; x < sobel.mag->width; x++) {
		for (int y = 0; y < sobel.mag->height; y++) {
			int ox = 0, oy = 0;
			switch(getPixelG8(sobel.dir, x, y))  {
				case 0: //0 degree
				case 240: // or 180
					ox = 0;
					oy = 1;
					break;
				case 60: //45 degree
					ox = 1;
					oy = -1;
					break;
				case 120: //90 degree
					ox = 1;
					oy = 0;
					break;
				case 180: //135 degree
					ox = 1;
					oy = 1;
					break;
				default:
					printf("INVALID DIRCODE AT (%d, %d)\n", x, y);
			}
			if ((getPixelG8(sobel.mag, x, y) < getPixelG8(sobel.mag, x-ox, y-oy)) || (getPixelG8(sobel.mag, x,y) < getPixelG8(sobel.mag, x+ox, y+oy)))
				setPixelG8(newmag, x, y, 0);
			else
				setPixelG8(newmag, x, y, getPixelG8(sobel.mag, x, y));
		}
	}

	avpicture_free((AVPicture *)sobel.mag);
	av_frame_free(&sobel.mag);

	sobel.mag = newmag;

	//	SaveFrameG8(sobel.mag, sobel.mag->width, sobel.mag->height, 50);
	//	SaveFrameG8(sobel.dir, sobel.dir->width, sobel.dir->height, 60);

	//Step 4:Hysteresis thresholding
	AVFrame * res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, sobel.mag->width, sobel.mag->height);
	res->width = sobel.mag->width;
	res->height = sobel.mag->height;
	
	//Appearantly it is possible that res "inherits" the data of a grayscale picture rather than being all black. Therefore, we have to do it ourselves. Again.
	memset(res->data[0], 0, res->linesize[0] * res->height);

	

	for (int x = 0; x < sobel.mag->width; x++) {
		for (int y = 0; y < sobel.mag->height; y++) {
			if (getPixelG8(sobel.mag, x, y) > HYSTERESIS_T1) {
				setPixelG8(res, x, y, getPixelG8(sobel.mag, x, y));
				setPixelG8(sobel.mag, x, y, 0);
				int ox = 0, oy = 0;
				switch(getPixelG8(sobel.dir, x, y))  {
					case 0: //0 degree
					case 240: // or 180
						ox = 1;
						oy = 1;
						break;
					case 60: //45 degree
						ox = 1;
						oy = 1;
						break;
					case 120: //90 degree
						ox = 0;
						oy = 1;
						break;
					case 180: //135 degree
						ox = -1;
						oy = 1;
						break;
				}
				//Follow edge in "+"-direction
				int ox2 = ox, oy2 = oy;
				while (getPixelG8(sobel.mag, x+ox2, y+oy2) > HYSTERESIS_T2) {
//					printf("Follow(+) edge to (%d,%d)\n", (x+ox2), (y+oy2));
					//Set this as edge pixel
					setPixelG8(res, x+ox2, y+oy2, getPixelG8(sobel.mag, x+ox2, y+oy2));
					setPixelG8(sobel.mag, x+ox2, y+oy2, 0);
					ox2 += ox;
					oy2 += oy;
				}
				//Repeat in "-"-direction
				ox2 = ox;
				oy2 = oy;
				while (getPixelG8(sobel.mag, x-ox2, y-oy2) > HYSTERESIS_T2) {
//					printf("Follow(-) edge to (%d,%d)\n", (x-ox2), (y-oy2));
					//Set this as edge pixel
					setPixelG8(res, x-ox2, y-oy2, getPixelG8(sobel.mag, x-ox2, y-oy2));
					setPixelG8(sobel.mag, x-ox2, y-oy2, 0);
					ox2 += ox;
					oy2 += oy;
				}
			//This position was not marked a pixel in res, but sodel output wasn't strong enough either...this is a memory artifact
		//	} else if (getPixelG8(res, x, y) < 255) {
		//		setPixelG8(res, x, y, 0);
			}
		}
	}
	//Free the rest and return result
	avpicture_free((AVPicture *)sobel.mag);
	av_frame_free(&sobel.mag);
	avpicture_free((AVPicture *)sobel.dir);
	av_frame_free(&sobel.dir);
	return res;
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
	
	//Smoothing operator and smoothed results
	OperatorMask * smoothing = getBellOperatorLinear(10);
	
	double * smoothed = malloc(sizeof(double) * diff_len);
	
	ListIterator * iter = list_iterate(list_frames);
	
	AVFrame * lastFrame;
	if (usefeedback) 
		lastFrame = getEdgeProfile(feedback->lastFrame, swsctx, width, height);
	else
		lastFrame = getEdgeProfile(list_next(iter), swsctx, width, height);

	AVFrame * thisFrame;
	int pos = (usefeedback?feedback->diff_len:0);
	while ((thisFrame = list_next(iter)) != NULL) {
		thisFrame = getEdgeProfile(thisFrame, swsctx, width, height);

		differences[pos++] = EdgeContrast(lastFrame)/2;

		av_frame_free(&lastFrame);
		lastFrame = thisFrame;
	}

	av_frame_free(&thisFrame);

	//create and fill new feedback array with unsmoothed values
	int fb_len = (list_frames->size < MAX_FEEDBACK_LENGTH?list_frames->size:MAX_FEEDBACK_LENGTH);

	double * newdiff = malloc(sizeof(double) * fb_len);
	for (int i = 0; i < fb_len; i++) {
		newdiff[i] = differences[(diff_len - fb_len + i)];
	}
	feedback->diff = newdiff;
	//Not yet; we still need to know how long the old feedback, which is now in differences, was
	//feedback->diff_len = fb_len;
	saveGraph(2700000+startframe, differences, diff_len);
	
	//apply smoothing

	//Convolve at borders
	for (int i = 0; i < smoothing->width; i++) {
		//!!!! Initialize field before blatantly adding onto it
		smoothed[i] = 0;
		//Add weighted values left of position i
		for (int j = 1; j <= smoothing->width; j++)
			if ((i - j) >= 0) smoothed[i] += smoothing->weights[smoothing->width + (j - 1)] * differences[i-j];
			else smoothed[i] += differences[i] * smoothing->weights[smoothing->width + (j - 1)];
		// No if-clause, we assume the rest will be long enough
		for (int j = 0; j < smoothing->width; j++)
			smoothed[i] += smoothing->weights[j] * differences[i + j];
	}
	for (int i = smoothing->width; i < (diff_len - smoothing->width); i++) {
		//!!!! Initialize field before blatantly adding onto it
		smoothed[i] = 0;
		//Add weighted values left of position i
		for (int j = 1; j <= smoothing->width; j++) {
			smoothed[i] += (smoothing->weights[smoothing->width + (j - 1)] * differences[i-j]);
		}

		for (int j = 0; j < smoothing->width; j++)
			smoothed[i] += (smoothing->weights[j] * differences[i + j]);
		
		if (smoothed[i] < 0.0 || smoothed[i] > 1.0)
			printf("ILLEGAL SMOOTHED VAR!(%f)\n", smoothed[i]);
	}
	
	for (int i = (diff_len - smoothing->width); i < diff_len; i++) {
			//!!!! Initialize field before blatantly adding onto it
			smoothed[i] = 0;
			//Add weighted values left of position i
			for (int j = 1; j <= smoothing->width; j++)
				smoothed[i] += smoothing->weights[smoothing->width + (j - 1)] * differences[i-j];
			// No if-clause, we assume the rest will be long enough
			for (int j = 0; j < smoothing->width; j++)
				if (i + j < (diff_len)) smoothed[i] += smoothing->weights[j] * differences[i + j];
				else smoothed[i] += differences[i] * smoothing->weights[j];
	}


	free(smoothing->weights);
	free(smoothing);


	//Switch arrays
	free(differences);
	differences = smoothed;

	saveGraph(3700000+startframe, differences, diff_len);
	
	//Start searching through differences to mark cuts
	double extr = 0;
	int asc = 1;
	for (int i = (usefeedback?feedback->diff_len:0); i < diff_len; i++) {
		if (asc) {
			if (differences[i] > extr) {
				extr = differences[i];
				asc = 0; //Descent to next minimum

				if (i >= feedback->diff_len) { // only push this result if we're beyond the feedback, otherwise we have a dupe result
					list_push(list_cuts, (void *)((intptr_t)(startframe + i)));
				}
			}
		} else if (differences[i] < extr) {
			extr = differences[i];
			asc = 1; //search next maximum again
		}
	}
	feedback->diff_len = fb_len;
	//for (int i = 0; i < fb_len; i++) printf("FEEDBACK[%d] = %f\n", i, feedback->diff[i]);
}

//Retrieves TOTAL image size in dimensions, parameters are then divided by amount of quadrants
//Thus, the parameters should be dividable by the set amount of quadrants in each respective dimension
//otherwise, some pixels at the right and lower border will be lost to the feature
InterpolationWeights * getLinearInterpolationWeights(int width, int height) {
	width /= QUADRANTS_WIDTH;
	height /= QUADRANTS_HEIGHT;
	InterpolationWeights * res = malloc(sizeof(InterpolationWeights));
	res->width = width;
	res->height = height;

//Allocate all the data in one bulk and assign pointers to the correct offsets in the bulk
	res->c = calloc(sizeof(double) * width * height, 9);
	res->nw = &res->c[width * height];
	res->n = &res->nw[width * height];
	res->ne = &res->n[width * height];
	res->e = &res->ne[width * height];
	res->se = &res->e[width * height];
	res->s = &res->se[width * height];
	res->sw = &res->s[width * height];
	res->w = &res->sw[width * height];

	//Define middle points of the surrounding quadrants
	int c_x = width / 2;
	int c_y = height / 2;

	int dx, dy;
	double wx, wy, wc, wax, way, wd; //Weight of gradients dx and dy, weight of center, weight of adjacent quadrant on x-axis and y-axis, diagonal quadrant's weight

	//Calculate distance(s) of each pixel in the center quadrant
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			// Gradients from center
			dx = x - c_x;
			dy = y - c_y;
			
			//Weight of gradients
			wx = fabs((double)dx / width);
			wy = fabs((double)dy / height);
			
			wc = (1.0 - wx) * (1.0 - wy);
			wax = wx * (1 - wy);
			way = (1 - wx) * wy;
			wd = wx * wy;
	
			double * P_ax = NULL; //Pointer to array of x-adjacent quadrant
			double * P_ay = NULL; //Pointer to array of y-adjacent quadrant
			double * P_d = NULL; //Pointer to array of diagonal quadrant

			//Determine which are the correspondending quadrants according to directions of dx and dy
			if (dx < 0) { //Western quadrants
				//x-adjacent is WEST
				P_ax = res->w;
				if (dy < 0) {
					//diagonal is NW
					P_d = res->nw;
					//y-adjacent is NORTH
					P_ay = res->n;
				} else if (dy > 0) {
					//diagonal is SW
					P_d = res->sw;
					//y-adjacent is SOUTH
					P_ay = res->s;
				}
			} else if (dx > 0) {
				//x-adjacent is EAST
				P_ax = res->e;
				if (dy < 0) {
					//diagonal is NE
					P_d = res->ne;
					//y-adjacent is NORTH
					P_ay = res->n;
				} else if (dy > 0) {
					//diagonal is SE
					P_d = res->se;
					//y-adjacent is SOUTH
					P_ay = res->s;
				}			
			} else { //in case of (dx == 0), specifically check for y-adjacent quadrant
				if (dy < 0) {
					//y-adjacent is NORTH
					P_ay = res->n;
				} else if (dy > 0) {
					//y-adjacent is SOUTH
					P_ay = res->s;
				}
			}
		
			setMatrixVar(res->c, wc, x, y, width);
			//These will have remained NULL depending on dx or dy being 0; therefore, only set the value if a quadrant has been selected
			if (P_ax) setMatrixVar(P_ax, wax, x, y, width);
			if (P_ay) setMatrixVar(P_ay, way, x, y, width);
			if (P_d)  setMatrixVar(P_d, wd, x, y, width);

		}
	}

	return res;
}

void getEdgeFeatures(AVFrame * frm, uint32_t * data, InterpolationWeights * weights) {
	double * values = calloc(sizeof(double), FEATURE_LENGTH);

	//Fill FEATURE_LENGTH values with the pixels in the correspondending quadrants in profile
	uint32_t q_width = frm->width / QUADRANTS_WIDTH;
	uint32_t q_height = frm->height / QUADRANTS_HEIGHT;

	double weightused;

	for (int qx = 0; qx < QUADRANTS_WIDTH; qx++) {
		for (int qy = 0; qy < QUADRANTS_HEIGHT; qy++) {
			// Get beginning of the quadrant
			int ox = qx * q_width;
			int oy = qy * q_height;
			//data[qx + qy * QUADRANTS_WIDTH] = 0;
			for (int x = 0; x < q_width; x++) {
				for (int y = 0; y < q_height; y++) {
					char touched = 0; //Bitmask telling which quadrants have been touched, clockwise starting from NW
					double w_c = 0.0, w_nw = 0.0, w_n = 0.0, w_ne = 0.0, w_e = 0.0, w_se = 0.0, w_s = 0.0, w_sw = 0.0, w_w = 0.0; //To avoid too much conversion, get the weights to be used beforehand and normalize them before convolving with the image using weightused
					weightused = (w_c = getMatrixVar(weights->c, x, y, q_width)); //center weight is always used
					//This should usually add up to 1, only in border quadrants it won't because their weight is not added to this variable

					
					if (qx > 0) {
						//Add to west
						//values[(qx-1) + qy * QUADRANTS_WIDTH] += getMatrixVar(weights->w, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						weightused += (w_w = getMatrixVar(weights->w, x, y, q_width));
						touched |= 1<<7;

						if (qy > 0) {
							//Add to Northwest
							//values[(qx-1) + (qy-1) * QUADRANTS_WIDTH] += getMatrixVar(weights->nw, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
							weightused += (w_nw = getMatrixVar(weights->nw, x, y, q_width));
							touched |= 1;
						} else if (qy < (QUADRANTS_HEIGHT - 1)) {
							//values[(qx-1) + (qy+1) * QUADRANTS_WIDTH] += getMatrixVar(weights->sw, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
							weightused += (w_sw = getMatrixVar(weights->sw, x, y, q_width));
							touched |= 1<<6;
						}
					}
					
					if (qx < (QUADRANTS_WIDTH - 1)) {
						//Add to east
						//values[(qx+1) + qy * QUADRANTS_WIDTH] += getMatrixVar(weights->e, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						weightused += (w_e = getMatrixVar(weights->e, x, y, q_width));
						touched |= 1<<3;

						if (qy > 0) {
							//Add to Northeast
							//values[(qx+1) + (qy-1) * QUADRANTS_WIDTH] += getMatrixVar(weights->ne, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
							weightused += (w_ne = getMatrixVar(weights->ne, x, y, q_width));
							touched |= 1<<2;
						} else if (qy < (QUADRANTS_HEIGHT - 1)) {
							//values[(qx+1) + (qy+1) * QUADRANTS_WIDTH] += getMatrixVar(weights->se, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
							weightused += (w_se = getMatrixVar(weights->se, x, y, q_width));
							touched |= 1<<4;
						}
					}

					if (qy > 0) {
						//Add to north
						//values[qx + (qy-1) * QUADRANTS_WIDTH] += getMatrixVar(weights->n, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						weightused += (w_n = getMatrixVar(weights->n, x, y, q_width)); 
						touched |= 1<<1;
					}

					if (qy < (QUADRANTS_HEIGHT - 1)) {
						//Add to south
						//values[qx + (qy+1) * QUADRANTS_WIDTH] += getMatrixVar(weights->s, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						weightused += (w_s = getMatrixVar(weights->s, x, y, q_width));
						touched |= 1<<5;
					}

					double pix = (double)getPixelG8(frm, x+ox, y+oy);
					//Normalize values by dividing by weightused
										  values[ qx    +  qy    * QUADRANTS_WIDTH] += (pix * (w_c  / weightused)); //Center
					if (touched & (1)   ) values[(qx-1) + (qy-1) * QUADRANTS_WIDTH] += (pix * (w_nw / weightused)); //NW
					if (touched & (1<<1)) values[(qx  ) + (qy-1) * QUADRANTS_WIDTH] += (pix * (w_n  / weightused)); //N
					if (touched & (1<<2)) values[(qx+1) + (qy-1) * QUADRANTS_WIDTH] += (pix * (w_ne / weightused)); // NE
					if (touched & (1<<3)) values[(qx+1) + (qy  ) * QUADRANTS_WIDTH] += (pix * (w_e  / weightused)); //E
					if (touched & (1<<4)) values[(qx+1) + (qy+1) * QUADRANTS_WIDTH] += (pix * (w_se / weightused)); //SE
					if (touched & (1<<5)) values[(qx  ) + (qy+1) * QUADRANTS_WIDTH] += (pix * (w_s  / weightused)); //S
					if (touched & (1<<6)) values[(qx-1) + (qy+1) * QUADRANTS_WIDTH] += (pix * (w_sw / weightused)); //SW
					if (touched & (1<<7)) values[(qx-1) + (qy  ) * QUADRANTS_WIDTH] += (pix * (w_w  / weightused)); //W
				}
			}
		}
	}

	for (int i = 0; i < FEATURE_LENGTH; i++)
		data[i] = (uint32_t)values[i];

}

void edgeFeatures_length(uint32_t * l) {
	*l = FEATURE_LENGTH;
}

void edgeFeatures(AVFrame * frm, uint32_t ** data, InterpolationWeights * w) {
	*data = (uint32_t *)malloc(sizeof(uint32_t) * FEATURE_LENGTH);
	getEdgeFeatures(frm, *data, w);

}
