#include <math.h>
#include "edgedetect.h"

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
	SaveFrameG8(sobel.mag, width, height, 3);
	SaveFrameG8(sobel.dir, width, height, 4);

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
				case 3 //135 degree:
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
	SaveFrameG8(sobel.mag, width, height, 5);

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
