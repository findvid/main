#!/usr/bin/python
# -*- coding: utf-8 -*-

import numpy as np
import cv2
import argparse
import os
import sys

IMGWIDTH = 640
IMGHEIGHT = 480

RASTERSIZE = 4

def errorMessage(msg):
	print '\033[91m' + msg + '\033[92m'

def infoMessage(msg):
	print msg

def successMessage(msg):
	print '\033[92m' + msg + '\033[92m'

def warningMessage(msg):
	print '\033[93m' + msg + '\033[92m'

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

	image = cv2.imread(imgPath)

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
	imgRaster = np.zeros((RASTERSIZE, RASTERSIZE, xRaster, yRaster, 3))

	x = 0
	for line in imgRaster:
		y = 0
		for field in line:
			imgRaster[x][y] = np.copy(imgResized[(x+0)*xRaster:(x+1)*xRaster, (y+0)*yRaster:(y+1)*yRaster])
			y+=1
		x+=1
