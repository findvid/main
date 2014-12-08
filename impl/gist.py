#!/usr/bin/python
# -*- coding: utf-8 -*-

import numpy as np
import cv2
import argparse
import os
import sys
import math
import cmath

IMGWIDTH = 64
IMGHEIGHT = 64

RASTERSIZE = 4

def errorMessage(msg):
	print '\033[91m' + msg + '\033[0m'

def infoMessage(msg):
	print msg

def successMessage(msg):
	print '\033[92m' + msg + '\033[0m'

def warningMessage(msg):
	print '\033[93m' + msg + '\033[0m'

def fourierTransform(image, N):
	F = np.zeros((N, N), dtype=np.complex64)
	P = np.zeros((N, N), dtype=np.complex64)

	# For each pixel in the NxN image
	for k in range(0, N):
		for b in range(0, N):
			Psum = complex(0)
			for a in range(0, N):
				Psum += (float(image[a][b]) * cmath.exp(-2j*cmath.pi * ((float(k*a)) / N)))
			
			P[k][b] = 1.0/N * Psum

	for k in range(0, N):
		for l in range(0, N):
			Fsum = complex(0)
			for b in range(0, N):
				Fsum += (P[k][b] * cmath.exp(-2j*cmath.pi * ((float(l*b)) / N)))
			
			F[k][l] = 1.0/N * Fsum

	return F


def logarithmicTransform(image):
	c = 255.0 / math.log(1 + math.fabs(np.amax(image)))

	logImage = np.zeros((image.shape[0], image.shape[1 ]))

	for x in range(0, logImage.shape[0]):
		for y in range(0, logImage.shape[1]):
			logImage[x][y] = c * math.log(1 + math.fabs(image[x][y]))

	return logImage

if __name__ == "__main__":

	##### Reading image via argument parser - just for testing purposes #####
	parser = argparse.ArgumentParser(description='Extracts gist features of a given image')
	parser.add_argument('image', metavar='IMG',
		help='An image for which the gist features are extracted')
	args = parser.parse_args()
	imgPath = args.image

	if not os.path.isfile(imgPath):
		errorMessage(imgPath + " - file not found")
		sys.exit(-1)

	splitext = os.path.splitext(os.path.basename(imgPath))
	filename = splitext[0]
	extension = splitext[1]

	image = cv2.imread(imgPath, cv2.CV_LOAD_IMAGE_GRAYSCALE)

	if (image is None) or (len(image) == 0):
		errorMessage("The file '" + imgPath + "' couldn't be read. (Not an image?)")
		sys.exit(-1)


	##### Scaling image #####
	infoMessage('Scaling image to: ' + str(IMGWIDTH) + 'x' + str(IMGHEIGHT))

	xFactor = float(IMGWIDTH) / image.shape[1]
	yFactor = float(IMGHEIGHT) / image.shape[0]

	infoMessage('x-factor: '+str(xFactor)+', y-factor: '+str(yFactor))

	imgResized = cv2.resize(image, (0,0), fx=xFactor, fy=yFactor)


	
	##### Split into raster #####
	yRaster = IMGWIDTH / RASTERSIZE
	xRaster = IMGHEIGHT / RASTERSIZE

	# create zero-filled raster
	imgRaster = np.zeros((RASTERSIZE, RASTERSIZE, xRaster, yRaster))

	for x in range(0, imgRaster.shape[0]-1):
		for y in range(0, imgRaster.shape[1]-1):
			imgRaster[x][y] = np.copy(imgResized[(x+0)*xRaster:(x+1)*xRaster, (y+0)*yRaster:(y+1)*yRaster])

	#for x in range(0, imgResized.shape[0]-1):
	#	for y in range(0, imgResized.shape[1]-1):
	#		imgResized[x][y] = (-1)**(x+y)

	fourier = fourierTransform(imgResized, IMGWIDTH)

	magnitude = np.zeros((IMGWIDTH, IMGWIDTH), dtype=np.float32)

	for k in range(0, magnitude.shape[0]):
		for l in range(0, magnitude.shape[1]):
			magnitude[k][l] = math.sqrt(fourier[k][l].real**2 + fourier[k][l].imag**2) 

	cv2.imwrite(filename + '_fourier' + extension, magnitude)
	cv2.imwrite(filename + '_fourierlog' + extension, logarithmicTransform(magnitude))

	##### Orientation Histogram #####
