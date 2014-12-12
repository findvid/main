#!/usr/bin/env python
from pymongo import MongoClient
#from subprocess import Popen, PIPE
import FindVid as fv
from sys import argv, exit
import hashlib
import os

def hashFile(filename, blocksize):
	hash = hashlib.sha1()
	with open(filename, 'rb') as f:
		buffer = f.read(blocksize)
		while len(buffer) > 0:
			hash.update(buffer)
			buffer = f.read(blocksize)
	
	return hash.hexdigest()

# returns the configuration dictionary
def config(db="findvid", collection="videos", config={"_id": "config"}):
	client = MongoClient()
	db = client[db]
	videos = db[collection]
	return videos.find(config).next()

CONFIG = config() # abs, thumbnail, video
VIDEOPATH = CONFIG["abspath"] + CONFIG["videopath"]

# path to shotbounds program
#SHOTBOUNDS = "{0}main/impl/shotbounds".format(CONFIG["abspath"])
#THUMBNAILER = "{0}main/impl/thumbnails".format(CONFIG["abspath"])

#Index the given videofile (rel. path), create thumbnails in designated folder or given alternative
def index_video(videofile, searchable=False, uploaded=True, thumbpath = None):
	#Get PyMongo client
	client = MongoClient()
	db = client["findvid"]
	videos = db["videos"]
	features = db["features"]

	vidpath = os.path.join(VIDEOPATH, videofile);

	#Get Hash
	fileHash = str(hashFile(vidpath, 65536))

	#Check if this exact video exists already
	video = videos.find_one({'_id': fileHash})
	if (video):
		return False

	#Use C-Lib to get cuts in the video
	cuts = fv.getCuts(vidpath)

	#Heuristic approach: Suitable keyframe between 2 cuts
	keyframes = [(cuts[i-1] + cuts[i])/2 for i in range(1, len(cuts))]

	#extract features from videofile given the keyframes array, use the middle keyframe as videothumb and save to default folder
	if (thumbpath == None):
		thumbpath = os.path.join(CONFIG["abspath"], CONFIG["thumbnailpath"])
	
	features = fv.getFeatures(vidpath, keyframes[len(keyframes)/2], keyframes, thumbpath)

	prev = 0
	scenes = [] # scenes collection
	for i, c in enumerate(cuts[1:]):
		scene = {} # scene document
		scene["_id"] = str(i)
		scene["startframe"] = prev
		scene["endframe"] = c
		scenes.append(scene)
		prev = c
	video = {}
	# TODO sequence counter
	video["_id"] = fileHash
	video["filename"] = videofile
	fps = fv.getFramerate(videofile)
	video["fps"] = fps
	video["framecount"] = cuts[-1:][0] # last entry
	video["scenes"] = scenes
	video["upload"] = uploaded
	video["searchable"] = searchable
	videos.insert(video)

	#Now write to features collection
	video = {}
	video["_id"] = fileHash
	scenes = [] # scenes collection
	for i, c in enumerate(cuts[1:]):
		scene = {} # scene document
		scene["_id"] = str(i)
		# save features
		scene["tinyimg"] = features[i][0]
		scene["edges"] = features[i][1]
		scene["colorhist"] = features[i][2]
		#scene["gist"] = features[i][3]
		scenes.append(scene)
	video["scenes"] = scenes
	features.insert(video)

	return True

if __name__ == "__main__":
	if len(argv) < 2:
		print "ERROR: file missing!"
		exit(1)
	videofile = argv[1]
	index_video(videofile)
