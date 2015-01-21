import numpy as npy
import random as rand
import sys
import Queue
import pickle
import math
import os.path
import multiprocessing
from pymongo import MongoClient

FILE_TREE = "_tree.db"
FILE_DEL = "_deletedVideos.db"
FILE_ADD = "_addedScenes.db"

def flattenFeatures(scene, weight):
	
	if not (weight >= 0 and weight <= 1):
		print ("Illegal weight parameter, defaulting to 0,5 / 0,5\n")
		weight = 0.5

	edgeweight = weight
	colorweight = 1 - weight

	maxweight = max(edgeweight, colorweight)

	colors = npy.array(scene["colorhist"])
	edges = npy.array(scene["edges"])
	
	#Normalize both features
	# f_norm = (f - f_mean) / f_deviation
	colors -= 500
	edges -= 981
	colors /= math.sqrt(4697656.84452)
	edges /= math.sqrt(2980531.28808)
	
	#Supersample colorhists to compensate for different length of vectors
	colors *= math.sqrt(2.5) # 320/128

	# "mean" distance is now 1; mutliply with sqrt(x) to project to 'x'
	# also, multiply features with their weight
	colors *= math.sqrt(colorweight / maxweight * 1000)
	edges *= math.sqrt(edgeweight / maxweight * 1000)
	result = colors.append(edges)
	
	return npy.array(scene['colorhist'])

class KMeansTree:
	isLeave = False
	center = []
	# When leave this field is abused for the data
	children = []

	def __init__(self, isLeave, center, children):
		self.isLeave = isLeave
		self.center = center
		self.children = children

	"""
	@param data		[(features, key),...] List of pairs of features and keys
	@param k		Giving the max amount of leaves a node should have
	@param maxiterations	limiting the iterations for the center finding
	@param recdepth
	"""
	def buildTree(self, data, k, maxiterations, recdepth = 0):
		# Output to get a feeling how long it takes (a long time)
		# 76 minutes for 1.000.000 entries with 1024 vectors
		align = ""
		for i in range(recdepth):
			align += "  "
		#print align, recdepth, ": len(data): ", len(data)

		# If there are less elements in data than a node should have children...
		if len(data) <= k:
			# Add all elements and become a leave node
			self.children = data
			self.isLeave = True
			return
		# Get random centers as starting point
		centers = calg(data,k)

		centersFound = False
		iterations = 0

		# Improve the centers
		while not centersFound and iterations < maxiterations:
			iterations += 1
			clusters = []

			for _ in range(len(centers)):
				clusters.append([])

			# cluster the points around the closest centers
			for position,value in data:
				index = 0
				mindist = sys.maxint
				for i,center in enumerate(centers):
					distance = dist(position, center)
					if distance < mindist:
						mindist = distance
						index = i
				clusters[index].append((position, value))

			centersNew = []
			# calculate new centers
			for cluster in clusters:
				# only if at least one point was added to the cluster it can be kept
				# TODO: this is a problem as certain data could cause all points to be in one cluster
				# like a data set of many equal entries
				# it would cause an not stopped recursion
				# Kind of fixed. See the Todo below
				if not cluster == []:
					center = npy.mean(npy.transpose([i[0] for i in cluster]), axis=1, dtype=npy.int)
					centersNew.append(center)

			# TODO Quick fix for now. If all points fall into one center they just become a child node
			# Might be problematic if there are many of them as that could slow down searches.
			# So they should be splitted in this case.
			if len(centersNew) == 1:
				print "Waring building a child node with", len(data), "entries"
				self.children = data
				self.isLeave = True
				return

			# Check if the centers changed
			if npy.array_equal(centers, centersNew):
				centersFound = True

			centers = centersNew
		for center,cluster in zip(centers,clusters):
			# Create a child for each cluster
			child = KMeansTree(False, center, [])
			# Fill it with values
			child.buildTree(cluster, k, maxiterations, recdepth+1)
			# Add chlid to children
			self.children.append(child)

	"""
	@param query		Feature array for the request. Find NNs to this one
	@param deletedVideos	Dictornary containing videos which shouldn't be found
	@param wantedNNs	Amount of NNs to be found
	@param maxPointsToTouch	Limit of leaves that get touched. Higher value -> better results but slower calculation
	"""
	def search(self, query, deletedVideos, wantedNNs = 1, maxPointsToTouch = 42):
		# for the nodes that get checked later
		nextNodes = Queue.PriorityQueue()
		# for the results
		results = Queue.PriorityQueue()

		# search from root
		self.traverse(nextNodes, results, query, deletedVideos)
		# while there are nextNodes and the max amount of points or the number of NNs is not reached
		while (not nextNodes.empty()) and (results.qsize() < maxPointsToTouch or results.qsize() < wantedNNs):
			# get the next clostest node
			_,nextNode = nextNodes.get()
			# and continue searching there
			nextNode.traverse(nextNodes, results, query, deletedVideos)
		return results

	"""
	@param nextNodes	PrioQueue for 'checkout-later'-nodes
	@param results		PrioQueue for the results
	@param query		Feature array for the request. Find NNs to this one.
	@param deletedVideos	Dictornary containing videos which shouldn't be found
	"""
	def traverse(self, nextNodes, results, query, deletedVideos):
		if self.isLeave:
			# put the leave to the results
			for center,value in self.children:
				if not value[0] in deletedVideos:
					results.put((dist(query, center),value))
		else:
			# find the closest child
			closestChild = None
			mindist = sys.maxint
			for child in self.children:
				distance = dist(child.center, query)
				if distance < mindist:
					# add the previously clostest child to the later to check nodes
					# doing that all but the actual closest child should get added
					if not closestChild == None:
						nextNodes.put((mindist,closestChild))
					mindist = distance
					# set new closest child
					closestChild = child
				else:
					# add the child to the later to check nodes
					nextNodes.put((distance,child))
			# go on searching in the closest child
			closestChild.traverse(nextNodes, results, query, deletedVideos)

	def __str__(self):
		return self.str2("")

	def str2(self,i):
		retval = i
		if self.isLeave:
			retval += i + "     [" + str(self.children) + " - " + str(self.center) + "]"
		else:
			retval += "Node:[" + str(self.center) + "\n"
			for c in self.children:
				retval += c.str2(i + "    ") + "\n"
			retval += i + "]"
		return retval

# Calculate the euclidian distance between two arrays
def dist(array1, array2):
	return npy.linalg.norm(array1 - array2)

# TODO: Crappy center selection algorithm. Should be replaced with something cooler.
def calg(arr,k):
	result = []
	N = 0

	for x,_ in arr:
		N += 1
		if len(result) < k:
			result.append(x)
		else:
			s = int(rand.random() * N)
			if s < k:
				result[s] = x
	return result

def buildChild(center, cluster, k, maxiterations, recdepth, results):
	child = KMeansTree(False, center, [])
	child.buildTree(cluster, k, maxiterations, recdepth+1)
	results.put(child)

class SearchHandler:
	# Name for the filenames
	name = None
	videos = None
	# KMeans-Tree
	tree = None
	# List for now
	addedScenes = []
	# Dict of all videos that shouldn't be found
	deletedVideos = dict()

	#not the actual maximal distance between vectors, but anything beyond this distance is no match at all
	max_dist = 1100.0 # average distance is normalized to 1000, something with average distance is a match of 10%

	"""
	Loads a tree from a file if the file exists, else it
	builds the tree from a given a collection containing videodata
	
	@param videos		The collection
	@param filename		filename of the tree
	@param k		Splittingfactor k
	@param imax		max iterations for the center finding
	@param forceRebuild	If true the tree will get rebuild no matter if the files exist
	"""
	def __init__(self, videos, name, featureWeight=0.5, k=8, imax=100, forceRebuild=False):
		self.name = name
		self.videos = videos
		# Try to load the tree from the file
		if os.path.isfile(self.name + FILE_TREE) and (not forceRebuild):
			print "Loading Tree from file"
			self.tree = pickle.load(open(self.name + FILE_TREE, "rb"))
			# Read files for deleted videos/added videos
			if os.path.isfile(self.name + FILE_DEL):
				self.deletedVideos = pickle.load(open(self.name + FILE_DEL, "rb"))
			if os.path.isfile(self.name + FILE_ADD):
				self.addedScenes = pickle.load(open(self.name + FILE_ADD, "rb"))
		# Build the tree
		else:
			print "Reading data from database"
			# Get all searchable videos. This also gets rid of the config entry
			vids = videos.find({'searchable' : True})

			data = []
			# Get all scenes for all searchable videos
			for vid in vids:
				scenes = vid['scenes']
				vidHash = vid['_id']
				for scene in scenes:
					sceneId = scene['_id']
					# Flatten the features
					feature = flattenFeatures(scene, featureWeight)
					data.append((feature,(vidHash,sceneId)))

			print "Building Tree"
			self.tree = KMeansTree(False, [], [])
			self.tree.buildTree(data, k, imax)
			
			print "Saving Tree"
			pickle.dump(self.tree, open(self.name + FILE_TREE, "wb"))
			pickle.dump(self.deletedVideos, open(self.name + FILE_DEL, "wb"))
			pickle.dump(self.addedScenes, open(self.name + FILE_ADD, "wb"))


	"""
	Search for a scene from a collection

	@param tree		kmeans tree to search on
	@param vidHash		id of the video of the query scene
	@param sceneId		id of the query scene
	@param wantedNNs	amount of NNs you want
	@param maxTouches	how many leaves should be touched at max. currently not different to wantedNNs

	@return			PrioriyQueue containing the results (>= wantedNNs if the tree is big enough)
	"""
	def search(self, vidHash, sceneId, wantedNNs=100, maxTouches=100):
		# Get feature of query scene
		vid = self.videos.find_one({'_id':vidHash})
		scene = vid['scenes'][sceneId]
		query = flattenFeatures(scene)
		# Search in the tree
		results = self.tree.search(query, self.deletedVideos, wantedNNs, maxTouches)
		# Add the newlyUploaded scenes to the results
		for feature,scene in self.addedScenes:
			results.put((dist(query,feature),scene))
		return results

	"""
	Add a video after the tree is build
	It will be kept in an extra list

	@param vidHash	hash of the video
	"""
	def addVideo(self, vidHash):
		vid = self.videos.find_one({'_id':vidHash})
		if vid['searchable']:
			if vidHash in self.deletedVideos:
				self.deletedVideos.pop(vidHash)
				pickle.dump(self.deletedVideos, open(self.name + FILE_DEL, "wb"))
			else:
				# Check if the video is on the addedScenes list already
				for _,(vidId,_) in self.addedScenes:
					if vidHash == vidId:
						return
				# If not add it to it
				scenes = vid['scenes']
				for scene in scenes:
					sceneId = scene['_id']
					feature = flattenFeatures(scene)
					self.addedScenes.append((feature,(vidHash,sceneId)))
				pickle.dump(self.addedScenes, open(self.name + FILE_ADD, "wb"))


	"""
	Disable a video so it can't be found anymore

	@param vidHash	hash of the video
	"""
	def deleteVideo(self, vidHash):
		# Add to the deleted videos list
		if not vidHash in self.deletedVideos:
			self.deletedVideos[vidHash] = True
			pickle.dump(self.deletedVideos, open(self.name + FILE_DEL, "wb"))
		# Delete it from the dynamic list
		addedScenesNew = []
		needsSaving = False
		for feature,(vidId,sceneNo) in self.addedScenes:
			if not vidHash == vidId:
				addedScenesNew.append((feature,(vidId,sceneNo)))
			else:
				needsSaving = True
		if needsSaving:
			self.addedScenes = addedScenesNew
			pickle.dump(self.addedScenes, open(self.name + FILE_ADD, "wb"))

if __name__ == '__main__':
	# Example code
	#"""
	client = MongoClient(port=8099)
	db = client["findvid"]
	videos = db["benchmark_tiny"]#oldvids"]#"small"]

	vid = videos.find_one({'filename':{'$regex':'.*mp4.*'}})

	searchHandler = SearchHandler(videos, "testvidhandler", forceRebuild=True)

	searchHandler.addVideo(vid['_id'])

	results = searchHandler.search(vid['_id'], 0, 100, 1000)

	print results.get()
	print results.get()
	print results.get()
	print results.get()
	print results.get()
	#"""
