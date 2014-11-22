#include <math.h>
#include "edgedetect.h"

void saveGraph(int id, double * vars, int vars_len){
	AVFrame * graph = av_frame_alloc();
	avpicture_alloc((AVPicture *)graph, PIX_FMT_RGB24, vars_len, 200);
	int numBytes = avpicture_get_size(PIX_FMT_RGB24, vars_len, 200);
	uint8_t * buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

	avpicture_fill((AVPicture *)graph, buffer, PIX_FMT_RGB24, vars_len, 200);
	for(int i = 0; i < vars_len; i++) {
		int var = (int)(200.0 - vars[i] * 200.0);
		for (int j = 200; j >= var; j--) 
			setPixel(graph, i, j, 0xff0000);
		printf("Plotting (%d,%d)\n", i, var);
	}
	SaveFrameRGB24(graph, vars_len, 200, id);
	av_free(graph);


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
		printf("Weight[%d] = %f\n", i, mask->weights[i]);
	}
	for (int i = width; i < (2 * width - 1); i++) {
		// (width - 1) - (2 * width - 2) = width - 1 - 2 * width + 2
		// = -width + 1
		int x = (width - 1) - i;
		mask->weights[i] = gauss((double)x, (width/3.0));
		printf("Weight[%d] = %f\n", x, mask->weights[i]);
	}
	return mask;
}

#define HYSTERESIS_T1 16
#define HYSTERESIS_T2 4

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

AVFrame * smoothGauss(AVFrame * gray, struct SwsContext * ctx, int width, int height) {
	AVFrame * smoothed = av_frame_alloc();
	avpicture_alloc((AVPicture *)smoothed, PIX_FMT_GRAY8, width, height);
	
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
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

	return smoothed;
}

/**
 * Convolve image with sodel operator
 */
struct t_sobelOutput {
	AVFrame * mag;
	AVFrame * dir;
};

void getSobelOutput(AVFrame * gray, struct t_sobelOutput * out, int width, int height) {
	AVFrame *mag = av_frame_alloc();
	avpicture_alloc((AVPicture *)mag, PIX_FMT_GRAY8, width, height);
	AVFrame *dir = av_frame_alloc();
	avpicture_alloc((AVPicture *)dir, PIX_FMT_GRAY8, width, height);


//For each pixel, apply sodel operator to obtain grayscale intensity changes in different directions
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			
			//Gradients of sodel operator
			int dx = getPixelG8(gray, x-1, y-1) + 2 * getPixelG8(gray, x-1, y) + getPixelG8(gray, x-1, y+1) - getPixelG8(gray, x+1, y-1) - 2 * getPixelG8(gray, x+1, y) - getPixelG8(gray, x+1, y+1);
			int dy = getPixelG8(gray,x-1,y-1) + 2 * getPixelG8(gray, x, y-1) + getPixelG8(gray, x+1, y-1) - getPixelG8(gray, x-1, y+1) - 2 * getPixelG8(gray, x, y+1) - getPixelG8(gray, x+1, y+1);

			//normalize output
			dx /= 9;
			dy /= 9;

			//each pixel saves the magnitude of the sodel operator at this point
 			double magvar = sqrt(dx * dx + dy * dy);
			mag->data[0][x + y * mag->linesize[0]] = (uint8_t)magvar;

			double dirvar = atan2(dx, dy);
			
			//Encode direction into a gray value: [-90|90] -> [0|3]
			//-180 = 0
			if (dirvar < 0) //]-90|0[ -> [180|90[
				dirvar = M_PI + dirvar;
			//[0|180] -> [0|255]
			//No need for drastic precision, use multiples of 45 degrees
			if (dirvar < 0 || dirvar > (M_PI))
				printf("invalid dirvar = %f\n", dirvar);

			dirvar = round(dirvar / (M_PI / 4));
			if (dirvar > 4.0)
				printf("invalid dircode = %f\n", dirvar);
			//dirvar *= (M_PI / 4);
			//dirvar = (dirvar/(M_PI)) * 255;


			dir->data[0][x + y * dir->linesize[0]] = (uint8_t)dirvar;
			//printf("dirvarf = %d\n", getPixelG8(dir, x, y));
		}
	}

	out->mag = mag;
	out->dir = dir;
}

AVFrame * getEdgeProfile(AVFrame * original, struct SwsContext * ctx, int width, int height) {
	// Step 0: Get grayscal picture
	AVFrame *gray = av_frame_alloc();
	avpicture_alloc((AVPicture *)gray,PIX_FMT_GRAY8, width, height);

	sws_scale(ctx, (const uint8_t * const*)original->data, original->linesize, 0, height, gray->data, gray->linesize);

	//Step 1: gaussian smoothing
	AVFrame * sgray = smoothGauss(gray, ctx, width, height);
	SaveFrameG8(sgray, width, height, 2);
	av_frame_free(&gray);

	//Step 2: Get SobelOutput
	struct t_sobelOutput sobel;
	getSobelOutput(sgray, &sobel, width, height);
	av_frame_free(&sgray);
	//SaveFrameG8(sobel.mag, width, height, 3);
	//SaveFrameG8(sobel.dir, width, height, 4);

	//Step 3: Non-maxmimum suppression
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < width; y++) {
			int ox, oy;
			switch(getPixelG8(sobel.dir, x, y))  {
				case 0: //0 degree
				case 4: // or 180
					ox = 0;
					oy = 1;
					break;
				case 1: //45 degree
					ox = 1;
					oy = -1;
					break;
				case 2: //90 degree
					ox = 1;
					oy = 0;
					break;
				case 3: //135 degree:
					ox = 1;
					oy = 1;
					break;
				default:
					printf("INVALID DIRCODE AT (%d, %d)\n", x, y);
			}
			if ((getPixelG8(sobel.mag, x, y) < getPixelG8(sobel.mag, x-oy, y-oy)) || (getPixelG8(sobel.mag, x,y) < getPixelG8(sobel.mag, x+ox, y+oy)))
				setPixelG8(sobel.mag, x, y, 0);
		}
	}

	linearScale(sobel.mag, width, height);
	//SaveFrameG8(sobel.mag, width, height, 5);

	//SaveFrameG8(sodel, width, height, 20);

	//Step 4:Hysteresis thresholding
	AVFrame * res = av_frame_alloc();
	avpicture_alloc((AVPicture *)res, PIX_FMT_GRAY8, width, height);


	for (int x = 0; x < width; x++) {
		for (int y = 0; y < width; y++) {
			if (getPixelG8(sobel.mag, x, y) > HYSTERESIS_T1) {
				setPixelG8(res, x, y, 255);
				setPixelG8(sobel.mag, x, y, 0);
				int ox = 0, oy = 0;
				switch(getPixelG8(sobel.dir, x, y))  {
					case 0: //0 degree
					case 4: // or 180
						ox = 1;
						oy = 0;
						break;
					case 1: //45 degree
						ox = 1;
						oy = 1;
						break;
					case 2: //90 degree
						ox = 0;
						oy = 1;
						break;
					case 3: //135 degree
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
			}
		}
	}
	//Free the rest and return result
	av_frame_free(&sobel.mag);
	av_frame_free(&sobel.dir);
	return res;
}

void detectCutsByEdges(LargeList * list_frames, LargeList * list_cuts, uint32_t startframe, ShotFeedback * feedback, struct SwsContext * swsctx, int width, int height) {
	//Store the difference values between each frame
	double * differences;
	int diff_len;

	//Smoothing operator
	OperatorMask * smoothing = getBellOperatorLinear(10);
	double * smoothed = malloc(sizeof(double) * diff_len);


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

		for ( int x = 0; x < width; x++ )
			for ( int y = 0; y < height; y++ ) {
				c1 += (getPixelG8(lastFrame, x, y)?1:0);
				c2 += (getPixelG8(thisFrame, x, y)?1:0);

				out += (getPixelG8(lastFrame, x, y) && !getPixelG8(thisFrame, x, y));
				in += (!getPixelG8(lastFrame, x, y) && getPixelG8(thisFrame, x, y));
			}

		differences[pos++] = fmax(1.0 * out / c1, 1.0 *  in / c2);
		av_free(lastFrame);
		lastFrame = thisFrame;
	}

	av_free(thisFrame);

	//create and fill new feedback array with unsmoothed values
	int fb_len = (list_frames->size < MAX_FEEDBACK_LENGTH?list_frames->size:MAX_FEEDBACK_LENGTH);

	double * newdiff = malloc(sizeof(double) * fb_len);
	for (int i = 0; i < fb_len; i++) {
		newdiff[i] = differences[(diff_len - fb_len + i)];
	}
	feedback->diff = newdiff;
	//Not yet; we still need to know how long the old feedback, which is now in differences, was
	//feedback->diff_len = fb_len;
printf("Saving unsmoothed graph\n");
	saveGraph(2700000+startframe, differences, diff_len);
printf("Saved unsmoothed graph\n");
	
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
printf("smoothed left border\n");

	for (int i = smoothing->width; i < (diff_len - smoothing->width); i++) {
		//!!!! Initialize field before blatantly adding onto it
		smoothed[i] = 0;
		//Add weighted values left of position i
		for (int j = 1; j <= smoothing->width; j++) {
			printf("Adding %f to smoothed[%d]\n", differences[i-j], i);
			smoothed[i] += (smoothing->weights[smoothing->width + (j - 1)] * differences[i-j]);
		}

		for (int j = 0; j < smoothing->width; j++)
			smoothed[i] += (smoothing->weights[j] * differences[i + j]);
		
		if (smoothed[i] < 0.0 || smoothed[i] > 1.0)
			printf("ILLEGAL SMOOTHED VAR!(%f)\n", smoothed[i]);
	}
printf("smoothed center\n");
	
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

printf("smoothed right border\n");

	free(smoothing->weights);
	free(smoothing);

	//Switch arrays
	free(differences);
	differences = smoothed;

printf("Saving smoothed graph\n");
	saveGraph(3700000+startframe, differences, diff_len);
printf("Saved smoothed graph\n");
	
	//Start searching through differences to mark cuts
	double extr = 0;
	int asc = 1;
	for (int i = (usefeedback?feedback->diff_len:0); i < diff_len; i++) {
		if (asc) {
			if (differences[i] > extr) {
				extr = differences[i];
				asc = 0; //Descent to next minimum

				if (i >= feedback->diff_len) // only push this result if we're beyond the feedback, otherwise we have a dupe result
					list_push(list_cuts, (void *)((startframe + i)));
			}
		} else if (differences[i] < extr) {
			extr = differences[i];
			asc = 1; //search next maximum again
		}
	}
	printf("Searched the bulk, setting feedback to length %d\n", fb_len);
	feedback->diff_len = fb_len;
	for (int i = 0; i < fb_len; i++) printf("FEEDBACK[%d] = %f\n", i, feedback->diff[i]);
}
