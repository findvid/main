#!/usr/bin/python
# -*- coding: utf-8 -*-

import numpy as np
import cv2
import argparse
import os
import sys
import math

IMGWIDTH = 32
IMGHEIGHT = 32

RASTERSIZE = 4

def errorMessage(msg):
	print '\033[91m' + msg + '\033[0m'

def infoMessage(msg):
	print msg

def successMessage(msg):
	print '\033[92m' + msg + '\033[0m'

def warningMessage(msg):
	print '\033[93m' + msg + '\033[0m'

def fourier(image, N):
	# F(x,y) = N-1 sum(i=0)( N-1 sum(j=0) (f(i,j)*e^-i*2*pi*( (x*i/N) + (y*j/N) )))
	# N = image of size NxN
	# f(i,j) = image in the spatial domain 
	# exp-term = basis function corresponding to each Point F(k,l) in the Fourier space
	# let's go...

	fourier = np.zeros((N, N))

	# For each pixel in the NxN image
	for x in range(0, N-1):
		for y in range(0, N-1):
			# Calculate fourier value
			for i in range(0, N-1):
				for j in range(0, N-1):
					fourier[x][y] += image[i][j] * math.exp( ((x*i)/N) + ((y*j)/N) )

	return fourier



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

	x = 0
	for line in imgRaster:
		y = 0
		for field in line:
			imgRaster[x][y] = np.copy(imgResized[(x+0)*xRaster:(x+1)*xRaster, (y+0)*yRaster:(y+1)*yRaster])
			y+=1
		x+=1

	cv2.imwrite(filename + '_fourier.' + extension, fourier(imgResized, 32))

	##### Orientation Histogram #####