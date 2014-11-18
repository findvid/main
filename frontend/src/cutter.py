#!/usr/bin/env python
from pymongo import MongoClient
from subprocess import Popen, PIPE
from sys import argv, exit
import os

# returns the configuration dictionary
def config(db="findvid", collection="videos", config={"_id": "config"}):
	client = MongoClient()
	db = client[db]
	videos = db[collection]
	return videos.find(config).next()

CONFIG = config() # abs, thumbnail, video
VIDEOPATH = CONFIG["abspath"] + CONFIG["videopath"]

# path to shotbounds program
SHOTBOUNDS = "{0}main/impl/shotbounds".format(CONFIG["abspath"])
THUMBNAILER = "{0}main/impl/thumbnails".format(CONFIG["abspath"])

# returns a list of cuts (framenumbers)
def cut(videofile):
	cmd = "{0} {1}/{2}".format(SHOTBOUNDS, VIDEOPATH, videofile)
	print cmd
	cuts = Popen(cmd, shell=True, stdout=PIPE).stdout.read().split()
	# if cuts == [0] then something went wrong
	if len(cuts) == 1 and cuts[0] == '0':
		print "Something went wrong; Correct filename/path?"
	return [int(cut) for cut in cuts] # convert strings to integers

def makethumbs(cuts, videofile):
	# ./thumbnails <videofile> <vidoe_thumb> <list of scene cuts>
	str_cuts = " ".join([str(x) for x in cuts[1:]])
	cmd = "{0} {1}/{2} 1 {3}".format(THUMBNAILER, VIDEOPATH, videofile, str_cuts)
	print cmd
	#for i, cut in enumerate(cuts):
	#	cmd = "ffmpeg -i {0}{1} -ss {2} -frames 1 {3}scene{4}.jpeg".format(VIDEOPATH+'/', videofile, int(cut)/25.0, "/video/videosearch/findvid/thumbnails/" + os.path.splitext(os.path.basename(videofile))[0] + '/', i)
	#	print cmd
	#	Popen(cmd, shell=True, stdout=PIPE).stdout.read().split()
	# not used: cmd = "ffmpeg -i {0}{1} -ss {2} -frames 1 {3}video{4}.jpeg".format(VIDEOPATH+'/', videofile, 1/25.0, "/video/videosearch/findvid/thumbnails/" + os.path.splitext(os.path.basename(videofile))[0] + '/', i)
	output = Popen(cmd, shell=True, stdout=PIPE).stdout.read().split()
	print output
#	if output != []: # error occured
#		exit(1)

def save_cuts(videofile, uploaded=False):
	cuts = cut(videofile)
	makethumbs(cuts, videofile)
	prev = 0 # start at frame 0
	scenes = [] # scenes collection
	for i, c in enumerate(cuts):
		scene = {} # scene document
		scene["_id"] = str(i)
		scene["startframe"] = prev
		scene["endframe"] = c
		scenes.append(scene)
		prev = c
	client = MongoClient()
	db = client["findvid"]
	videos = db["videos"]
	video = {}
	# TODO sequence counter
	video["_id"] = str(videos.find().count() - 1)
	video["filename"] = videofile
	video["fps"] = 25
	video["framecount"] = cuts[-1:][0] # last entry
	video["scenes"] = scenes
	video["upload"] = uploaded
	videos.insert(video)

if __name__ == "__main__":
	if len(argv) < 2:
		print "ERROR: file missing!"
		exit(1)
	videofile = argv[1]
	save_cuts(videofile)
