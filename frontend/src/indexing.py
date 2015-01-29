#!/usr/bin/env python
import time
from pymongo import MongoClient
#from subprocess import Popen, PIPE
import FindVid as fv
from sys import argv, exit
import hashlib
import os
import subprocess, shlex

def hashFile(filename, blocksize):
	hash = hashlib.sha1()
	# File not found error is thrown up(wards)
	with open(filename, 'rb') as f:
		buffer = f.read(blocksize)
		while len(buffer) > 0:
			hash.update(buffer)
			buffer = f.read(blocksize)

	return str(hash.hexdigest())

# returns the configuration dictionary
def config(db="findvid", collection="videos", config={"_id": "config"}):
	client = MongoClient(port=8099)
	db = client[db]
	videos = db[collection]
	config = videos.find_one(config)
	videopath = config["abspath"] + config["videopath"]

	return (videos, videopath)

def transcode_video(srcVideo, dstVideo, quiet=False):
	quietText = ""
	if quiet:
		quietText = " -loglevel quiet"

	cmd = "ffmpeg -y -i " + srcVideo + " -c:v libx264" + quietText + " -preset veryslow " + dstVideo
	if not quiet:
		print (cmd)
	subprocess.call(cmd,shell=True)

#Index the given videofile (rel. path), create thumbnails in designated folder or given alternative
def index_video(database, collection, videofile, searchable=True, uploaded=False, thumbpath = None):

	videos, videopath = config(db=database, collection=collection)

	vidpath = os.path.join(videopath, videofile);

	#Get Hash
	fileHash = hashFile(vidpath, 65536)
	#if (fileHash is None): return False

	#Check if this exact video exists already
	video = videos.find_one({'_id': fileHash})
	if (video):
		if video['removed']:
			videos.update({'_id': fileHash}, {'$set': {'removed': False}})
			return fileHash
		else:
			return None

	#Use C-Lib to get cuts in the video
	cuts = fv.getCuts(vidpath)
	#Heuristic approach: Suitable keyframe between 2 cuts
	keyframes = [(cuts[i-1] + cuts[i])/2 for i in range(1, len(cuts))]

	#extract features from videofile given the keyframes array, use the middle keyframe as videothumb and save to default folder
	if (thumbpath == None):
		thumbpath = os.path.join(CONFIG["abspath"], CONFIG["thumbnailpath"])
	
	features = fv.getFeatures(vidpath, fileHash, keyframes[len(keyframes)/2], keyframes, thumbpath)

	scenes = [] # features of scenes as list
	for i, c in enumerate(cuts[1:]):
		scene = {} # scene document
		scene["_id"] = i
		scene["tinyimg"] = features[i][0]
		scene["edges"] = features[i][1]
		scene["colorhist"] = features[i][2]
		scenes.append(scene)
	video = {}
	
	#General video information
	video["_id"] = fileHash
	video["filename"] = videofile
	video["uploadtime"] = time.time()

	fps = fv.getFramerate(vidpath)
	video["fps"] = fps
	
	
	video["cuts"] = cuts
	video["scenes"] = scenes

	video["upload"] = uploaded
	video["searchable"] = searchable

	#The momentous step of inserting into the database
	#This is done on a single document(Or is it?) and therefore atomic, according to the documentation
	#therefore, user induced process abortion should not leave anything to be cleaned up
	videos.insert(video)


	return fileHash

if __name__ == "__main__":
	if len(argv) < 2:
		print "ERROR: file missing!" + "\n"
		exit(1)
	#Get PyMongo client
	client = MongoClient(port=8099)
	db = client["findvid"]
	videos = db["videos"]
	
	videofile = argv[1]
	index_video(videos, videofile)
