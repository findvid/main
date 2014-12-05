#include <math.h>
#include "fvutils.h"
#include "largelist.h"
#include "edgedetect.h"

void saveGraph(int id, double * vars, int vars_len){
printf("SAVE GRAPH %d\n", id);
	AVFrame * graph = av_frame_alloc();
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
	av_frame_free(&graph);
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
			dirvar = ((dirvar/(M_PI)) * 240);

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

	SaveFrameG8(sgray, sgray->width, sgray->height, 40);

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

		SaveFrameG8(sobel.mag, sobel.mag->width, sobel.mag->height, 50);
		SaveFrameG8(sobel.dir, sobel.dir->width, sobel.dir->height, 60);

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
				setPixelG8(res, x, y, 255);
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
					setPixelG8(res, x+ox2, y+oy2, 255);
					//No need to repeat this
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
					setPixelG8(res, x-ox2, y-oy2, 255);
					//No need to repeat this
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

		int c1 = 0;
		int c2 = 0;

		int out = 0;
		int in = 0;
		for ( int x = 0; x < thisFrame->width; x++ )
			for ( int y = 0; y < thisFrame->height; y++ ) {
				c1 += (getPixelG8(lastFrame, x, y)?1:0);
				c2 += (getPixelG8(thisFrame, x, y)?1:0);

				out += (getPixelG8(lastFrame, x, y) && !getPixelG8(thisFrame, x, y));
				in += (!getPixelG8(lastFrame, x, y) && getPixelG8(thisFrame, x, y));
			}
		if (c1 || c2)
			differences[pos++] = fmax(1.0 * out / c1, 1.0 *  in / c2);
		else 
			differences[pos++] = 0.0;
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
	}

printf("smoothed\n");

	free(smoothing->weights);
printf("freed weights\n");
	free(smoothing);
printf("freed mask struct\n");


	//Switch arrays
	free(differences);
printf("freed diffs\n");
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

InterpolationWeights * getLinearInterpolationWeights(int width, int height) {
	InterpolationWeights * res = malloc(sizeof(InterpolationWeights));
	res->width = width;
	res->height = height;

//Allocate all the data in one bulk and assign pointers to the correct offsets in the bulk
	res->c = malloc(sizeof(double) * width * height * 9);
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
	int nw_x = c_x - width;
	int nw_y = c_y - height;
	int n_x = c_x;
	int n_y = c_y - height;
	int ne_x = c_x + width;
	int ne_y = c_y - height;
	int e_x = c_x + width;
	int e_y = c_y;
	int se_x = c_x + width;
	int se_y = c_y + height;
	int s_x = c_x;
	int s_y = c_y + height;
	int sw_x = c_x - width;
	int sw_y = c_y + height;
	int w_x = c_x - width;
	int w_y = c_y;

	int dx, dy;
	double dist, w;
	
	//double maxdist = fmin(width, height);
	double maxdist = sqrt(width * width + height * height);
	//Currently assume that width and height are the same

	//To use different width and height, height and dy have to be multiplied
	//by the ratio of (width/height) so that height = width and dy is as long as it would be in that case
	//thus artificially expanding distances in y-direction to reduce to the case of width == height, which is considered here

	//Calculate distance from each pixel in the center quadrant
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			//distance to NW center
			dx = nw_x - x;
			dy = nw_y - y;
			dist = sqrt(dx * dx + dy * dy);
			//dist = fmax(fabs(nw_x - x),fabs(nw_y - y));
			w = (0.5/3) * (1.0 - dist/maxdist);
			if (!w) w = 0; //Negative value means dist was too far away
			setMatrixVar(res->nw, w, x, y, width);

			//distance to N center
			dx = n_x - x;
			dy = n_y - y;
			dist = sqrt(dx * dx + dy * dy);
			//dist = fabs(n_y - y);
			w = (0.5/3) * (1.0 - dist/maxdist);
			if (!w) w = 0; //Negative value means dist was too far away
			setMatrixVar(res->n, w, x, y, width);

			// NE
			dx = ne_x - x;
			dy = ne_y - y;
			dist = sqrt(dx * dx + dy * dy);
			//dist = fmax(fabs(ne_x - x),fabs(ne_y - y));
			w = (0.5/3) * (1.0 - dist/maxdist);
			if (!w) w = 0; //Negative value means dist was too far away
			setMatrixVar(res->ne, w, x, y, width);

			// E
			dx = e_x - x;
			dy = e_y - y;
			dist = sqrt(dx * dx + dy * dy);
			//dist = fabs(e_x - x);
			w = (0.5/3) * (1.0 - dist/maxdist);
			if (!w) w = 0; //Negative value means dist was too far away
			setMatrixVar(res->e, w, x, y, width);

			//SE
			dx = se_x - x;
			dy = se_y - y;
			dist = sqrt(dx * dx + dy * dy);
			//dist = fmax(fabs(se_x - x),fabs(se_y - y));
			w = (0.5/3) * (1.0 - dist/maxdist);
			if (!w) w = 0; //Negative value means dist was too far away
			setMatrixVar(res->se, w, x, y, width);

			//S
			dx = s_x - x;
			dy = s_y - y;
			dist = sqrt(dx * dx + dy * dy);
			w = (0.5/3) * (1.0 - dist/maxdist);
			if (!w) w = 0; //Negative value means dist was too far away
			setMatrixVar(res->s, w, x, y, width);

			//SW
			dx = sw_x - x;
			dy = sw_y - y;
			dist = sqrt(dx * dx + dy * dy);
			w = (0.5/3) * (1.0 - dist/maxdist);
			if (!w) w = 0; //Negative value means dist was too far away
			setMatrixVar(res->sw, w, x, y, width);

			//W
			dx = w_x - x;
			dy = w_y - y;
			dist = sqrt(dx * dx + dy * dy);
			w = (0.5/3) * (1.0 - dist/maxdist);
			if (!w) w = 0; //Negative value means dist was too far away
			setMatrixVar(res->w, w, x, y, width);

			//Center
			dx = c_x - x;
			dy = c_y - y;
			dist = sqrt(dx * dx + dy * dy);
			w = 1.0 - (0.5 * dist / maxdist);
			setMatrixVar(res->c, w, x, y, width);
		}
	}

	return res;
}

void getEdgeFeatures(AVFrame * frm, uint32_t * data, InterpolationWeights * weights) {
	double * values = calloc(sizeof(double), FEATURE_LENGTH);

	//Fill FEATURE_LENGTH values with the pixels in the correspondending quadrants in profile
	uint32_t q_width = frm->width / QUADRANTS_WIDTH;
	uint32_t q_height = frm->height / QUADRANTS_HEIGHT;

	for (int qx = 0; qx < QUADRANTS_WIDTH; qx++) {
		for (int qy = 0; qy < QUADRANTS_HEIGHT; qy++) {
			// Get beginning of the quadrant
			int ox = qx * q_width;
			int oy = qy * q_height;
			//data[qx + qy * QUADRANTS_WIDTH] = 0;
			for (int x = 0; x < q_width; x++) {
				for (int y = 0; y < q_height; y++) {
					//Strict separation, can and should be improved by bilinear interpolation
					//data[qx + qy * QUADRANTS_WIDTH] += (getPixelG8(profile, x+ox, y+oy)?1:0);
					if (qx > 0) {
						//Add to west
						values[(qx-1) + qy * QUADRANTS_WIDTH] += getMatrixVar(weights->w, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						if (qy > 0) {
							//add to north west
							values[(qx-1) + (qy-1) * QUADRANTS_WIDTH] += getMatrixVar(weights->nw, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						}
						if (qy < (QUADRANTS_HEIGHT - 1)) {
							//Add to south west
							values[(qx-1) + (qy+1) * QUADRANTS_WIDTH] += getMatrixVar(weights->nw, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						}
					}

					if (qy > 0) {
						//Add to north
						values[qx + (qy-1) * QUADRANTS_WIDTH] += getMatrixVar(weights->nw, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
					}

					if (qy < (QUADRANTS_HEIGHT - 1)) {
						//Add to south
						values[qx + (qy+1) * QUADRANTS_WIDTH] += getMatrixVar(weights->nw, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
					}

					if (qx < (QUADRANTS_WIDTH - 1)) {
						//Add to east
						values[(qx+1) + qy * QUADRANTS_WIDTH] += getMatrixVar(weights->w, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						if (qy > 0) {
							//add to north east
							values[(qx+1) + (qy-1) * QUADRANTS_WIDTH] += getMatrixVar(weights->nw, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						}
						if (qy < (QUADRANTS_HEIGHT - 1)) {
							//Add to south east
							values[(qx+1) + (qy+1) * QUADRANTS_WIDTH] += getMatrixVar(weights->nw, x, y, q_width) * getPixelG8(frm, x+ox, y+oy);
						}
					}
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
