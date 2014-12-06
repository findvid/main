#!/usr/bin/python
# -*- coding: utf-8 -*-

import numpy as np
import cv2
import argparse
import os
import sys

IMGWIDTH = 640
IMGHEIGHT = 480

def errorMessage(msg):
	print '\033[91m' + msg + '\033[92m'

def infoMessage(msg):
	print msg

def successMessage(msg):
	print '\033[92m' + msg + '\033[92m'

def warningMessage(msg):
	print '\033[93m' + msg + '\033[92m'

if __name__ == "__main__":
	# argument parser - just for testing purposes
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

	imageResized = cv2.resize(image, (0,0), fx=xFactor, fy=yFactor)


	#####  #####

	cv2.imwrite('./'+filename+'_resized.'+extension, imageResized)
