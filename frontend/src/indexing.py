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

#Index the given videofile (rel. path), create thumbnails in designated folder or given alternative
def index_video(collection, videofile, searchable=True, uploaded=False, thumbpath = None):

	vidpath = os.path.join(VIDEOPATH, videofile);

	#Get Hash
	fileHash = str(hashFile(vidpath, 65536))

	#Check if this exact video exists already
	video = videos.find_one({'_id': fileHash})
	if (video):
		return None

	#Use C-Lib to get cuts in the video
	cuts = fv.getCuts(vidpath)

	#Heuristic approach: Suitable keyframe between 2 cuts
	keyframes = [(cuts[i-1] + cuts[i])/2 for i in range(1, len(cuts))]

	#extract features from videofile given the keyframes array, use the middle keyframe as videothumb and save to default folder
	if (thumbpath == None):
		thumbpath = os.path.join(CONFIG["abspath"], CONFIG["thumbpath"])
	
	features = fv.getFeatures(vidpath, fileHash, keyframes[len(keyframes)/2], keyframes, thumbpath)

	scenes = [] # features of scenes as list
	for i, c in enumerate(cuts[1:]):
		scene = {} # scene document
		scene["_id"] = str(i)
		scene["tinyimg"] = features[i][0]
		scene["edges"] = features[i][1]
		scene["colorhist"] = features[i][2]
		scenes.append(scene)
	video = {}
	
	#General video information
	video["_id"] = fileHash
	video["filename"] = videofile
	video["uploadtime"] = time.time()

	fps = fv.getFramerate(videofile)
	video["fps"] = fps
	
	
	video["cuts"] = cuts
	video["scenes"] = scenes

	video["upload"] = uploaded
	video["searchable"] = searchable

	#The momentous step of inserting into the database
	#This is done on a single document(Or is it?) and therefore atomic, according to the documentation
	#therefore, user induced process abortion should not leave anything to be cleaned up
	collection.insert(video)


	return fileHash

if __name__ == "__main__":
	if len(argv) < 2:
		print "ERROR: file missing!"
		exit(1)
	#Get PyMongo client
	client = MongoClient()
	db = client["findvid"]
	videos = db["videos"]
	
	videofile = argv[1]
	index_video(videos, videofile)
