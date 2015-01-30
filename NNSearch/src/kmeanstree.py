import numpy as npy
import random as rand
import sys
import Queue
import pickle
import math
import os.path
import math
import threading
import processhandler
import time
from pymongo import MongoClient

FILE_TREE = "_tree.db"
FILE_DEL = "_deletedVideos.db"
FILE_ADD = "_addedScenes.db"

lock = threading.Lock()

"""
Returns children for the given arguments.
They are most of the times don't have childs
of there own yet but if so there is a second
return value with the data the subchilds will
be made off.

@param data		[(features, key),...] List of pairs of features and keys
@param k		Giving the max amount of leaves a node should have
@param maxiterations	limiting the iterations for the center finding
@param recdepth		Don't mind me. Debuggin reasons

@return 		[(child, data)], where data is None if the child is a leave
"""
def buildTree(data, k, maxiterations, recdepth = 0):
	# Output to get a feeling how long it takes (a long time)
	# 76 minutes for 1.000.000 entries with 1024 vectors
	align = ""
	for i in range(recdepth):
		align += "  "
	print align, recdepth, ": len(data): ", len(data)

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
			if not cluster == []:
				center = npy.mean(npy.transpose([i[0] for i in cluster]), axis=1, dtype=npy.int)
				centersNew.append(center)

		if len(centersNew) == 1:
			tmp = [[] for i in range(k)]
			for i,v in enumerate(clusters[0]):
				tmp[i%k].append(v)

			res = []
			for cluster in tmp:
				if len(cluster) < k:
					child = KMeansTree(True, center, cluster)
					res.append((child, None))
				else:
					child = KMeansTree(False, center, [])
					res.append((child, cluster))
			return res

		# Check if the centers changed
		if npy.array_equal(centers, centersNew):
			centersFound = True

		centers = centersNew
	res = []
	for center,cluster in zip(centers,clusters):
		if len(cluster) < k:
			# Create a child for each cluster
			child = KMeansTree(True, center, cluster)
			res.append((child, None))
		else:
			child = KMeansTree(False, center, [])
			res.append((child, cluster))
	return res

"""
Builds a tree, multithreaded (processed)

@param result		Result of the last build. So it't really rather the tasts that there are
@param parent		Parent of the childs in result
@param processHandler	Object to create processes
@param k		k the split factor for the tree
@param maxiterations	maxiterations factor for the tree
@param recdepth		Debugging reasons
"""
def treeBuilder(result, parent, processHandler, k, maxiterations, recdepth=0):
	for (tree,data) in result:
		if not tree.isLeave:
			if len(data) < 42:
				res = buildTree(data=data, k=k, maxiterations=maxiterations, recdepth=recdepth)
				treeBuilder(result=res, parent=tree, processHandler=processHandler, k=k, maxiterations=maxiterations, recdepth=recdepth)
			else:
				processHandler.runTask(priority=1, onComplete=treeBuilder, onCompleteArgs=(), onCompleteKwargs={"parent":tree, "processHandler":processHandler, "k":k, "maxiterations":maxiterations, "recdepth":recdepth}, target=buildTree, args=(), kwargs={"data":data, "k":k, "maxiterations":maxiterations, "recdepth":recdepth}, name=None)
	lock.acquire()
	try:
		for (tree,_) in result:
			if parent != None:
				parent.children.append(tree)
	finally:
		lock.release()

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
	Searches in the tree

	@param query		Feature array for the request. Find NNs to this one
	@param deletedVideos	Dictornary containing videos which shouldn't be found
	@param wantedNNs	Amount of NNs to be found
	@param maxPointsToTouch	Limit of leaves that get touched. Higher value -> better results but slower calculation

	@return			Priority Queue with the results
	"""
	def search(self, query, deletedVideos, wantedNNs=1, maxPointsToTouch=42):
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
		res = []
		for i in range(wantedNNs):
			if results.empty():
				return res
			res.append(results.get())
		return res

	"""
	Travereses the tree for the search

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

	"""
	To String for debugging
	"""
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

class SearchHandler:
	# Name for the filenames
	name = None
	# Collection containing the videos
	videos = None
	# Weigth for the faltening
	featureWeight = 0.5
	# KMeans-Tree
	tree = None
	# List for now
	addedScenes = []
	# Dict of all videos that shouldn't be found
	deletedVideos = dict()
	# ProcessHander for multiprocessing
	processHandler = None
	# Shadow copy to keep it updated
	shadowCopy = None

	#not the actual maximal distance between vectors, but anything beyond this distance is no match at all
	max_dist = 1100.0 # average distance is normalized to 1000, something with average distance is a match of 10%

	"""
	Loads a tree from a file if the file exists, else it
	builds the tree from a given a collection containing videodata
	Use self.processHandler.waitForPriority(priority=1, waitTime=10) to wait till the tree is fully build.
	This module assumes that you never build more than one tree at once.
	
	@param videos		The collection
	@param filename		filename of the tree
	@param k		Splittingfactor k
	@param imax		max iterations for the center finding
	@param forceRebuild	If true the tree will get rebuild no matter if the files exist
	"""
	def __init__(self, videos, name, featureWeight=0.5, processHandler=None):
		if not (featureWeight >= 0.0 and featureWeight <= 1.0):
			print ("Illegal weight parameter (" + str(featureWeight) + "), defaulting to 0.5/0.5\n")
			featureWeight = 0.5
		self.name = name
		self.videos = videos
		self.featureWeight = featureWeight
		self.processHandler = processHandler
		self.shadowCopy = None
		self.addedScenes = []
		self.deletedVideos = dict()

	"""
	Loads the tree or builds it if needed/requested

	@param k		Splitfactor for the tree
	@param imax		iterations factor for the tree
	@param forceRebuild	if true the tree won't be loaded even if one exists
	"""
	def loadOrBuildTree(self, k=8, imax=100, forceRebuild=False):
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
			data = self.processHandler.runTaskWait(priority=1, target=self.readFromDB, kwargs={"db":self.videos.database.name, "collection":self.videos.name})

			print "Building Tree"
			self.tree = KMeansTree(False, [], [])
			treeBuilder(result=[(self.tree, data)], processHandler=self.processHandler, parent=None, k=k, maxiterations=imax)

			self.processHandler.waitForPriority(priority=1, waitTime=10)
			#time.sleep(200)

			print "Saving Tree"
			pickle.dump(self.tree, open(self.name + FILE_TREE, "wb"))
			pickle.dump(self.deletedVideos, open(self.name + FILE_DEL, "wb"))
			pickle.dump(self.addedScenes, open(self.name + FILE_ADD, "wb"))

	"""
	Reads the feature data from the database and flattens it.

	@param db		name of the batabase
	@param collection	name of the collection

	@return			a list like [(feature,(vidHash,sceneId))]
				where
				feature is the flattend features of the scene
				vidHash the hash of the video
				sceneId the ID of the scene in the video
	"""
	def readFromDB(self, db, collection):
		client = MongoClient(port=8099)
		db = client[db]
		videos = db[collection]

		vids = videos.find({'searchable' : True})

		data = []
		# Get all scenes for all searchable videos
		for vid in vids:
			scenes = vid['scenes']
			vidHash = vid['_id']
			for scene in scenes:
				sceneId = scene['_id']
				# Flatten the features
				feature = self.flattenFeatures(scene)
				data.append((feature,(vidHash,sceneId)))
		return data

	"""
	Flattens the features for a scenen

	@param scene		the scene containing the different features

	@return 		the flattend features
	"""
	def flattenFeatures(self, scene):
		edgeweight = self.featureWeight
		colorweight = 1 - self.featureWeight

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
		#colors *= math.sqrt(2.8125) # 360/128 That's what is should have been
		colors *= math.sqrt(2.5) # 320/128 #actually not correct magic number, but benchmark has optimized this number
		# Which is actually fine because it just means the weighting basicallz does the same kind of operation afterwards

		# "mean" distance is now 1; mutliply with sqrt(x) to project to 'x'
		# also, multiply features with their weight
		colors *= math.sqrt(colorweight / maxweight * 1000)
		edges *= math.sqrt(edgeweight / maxweight * 1000)
		result = npy.append(colors, edges)
		
		return result #npy.array(scene['colorhist'])

	"""
	Calculates a precent value for a distance

	@param dist		distance in the tree

	@return 		a value between 0 and 100
	"""
	def distQuality(self, dist):
		# 1000 fits well with the way our flattening works
		v = (1 - (dist/1000))
		return max(v, 0)

	"""
	Search for a scene from a collection

	@param tree		kmeans tree to search on
	@param vidHash		id of the video of the query scene
	@param sceneId		id of the query scene
	@param wantedNNs	amount of NNs you want
	@param maxTouches	how many leaves should be touched at max. currently not different to wantedNNs

	@return			PrioriyQueue containing the results (>= wantedNNs if the tree is big enough)
	"""
	def search(self, vidHash, sceneId, wantedNNs=100, maxTouches=100, filterChecked=False):
		# Get feature of query scene
		vid = self.videos.find_one({'_id':vidHash})
		scene = vid['scenes'][sceneId]
		query = self.flattenFeatures(scene)
		# Copy the list of videos which won't be found and add the source Video
		toIgnore = self.deletedVideos.copy()
		if not filterChecked:
			toIgnore[vidHash] = True
		# Search in the tree
		results = self.processHandler.runTaskWait(priority=3, target=self.tree.search, args=(query, toIgnore, wantedNNs, maxTouches))
		resqueue = Queue.PriorityQueue()

		for result in results:
			resqueue.put(result)

		# Add the newlyUploaded scenes to the results
		for feature,(video, scene) in self.addedScenes:
			if filterChecked or (video != vidHash):
				resqueue.put((dist(query,feature),(video, scene)))

		results = []
		for i in range(wantedNNs):
			results.append(resqueue.get())

		return results

	"""
	Add a video after the tree is build
	It will be kept in an extra list

	@param vidHash	hash of the video
	"""
	def addVideo(self, vidHash):
		# Keep the ShadowCopy updated
		if self.shadowCopy != None:
			self.shadowCopy.addVideo(vidHash)
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
					feature = self.flattenFeatures(scene)
					self.addedScenes.append((feature,(vidHash,sceneId)))
				pickle.dump(self.addedScenes, open(self.name + FILE_ADD, "wb"))


	"""
	Disable a video so it can't be found anymore

	@param vidHash	hash of the video
	"""
	def deleteVideo(self, vidHash):
		# Keep the ShadowCopy updated
		if self.shadowCopy != None:
			self.shadowCopy.deleteVideo(vidHash)
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
	videos = db["benchmark"]#_tiny"]#oldvids"]#"small"]

	vid = videos.find_one({'filename':{'$regex':'.*target.*'}})

	searchHandler = SearchHandler(videos, "testvidhandler", processhandler=processhandler.ProcessHandler(), forceRebuild=True)

	searchHandler.addVideo(vid['_id'])

	results = searchHandler.search(vid['_id'], 0, 100, 1000, True)

	for i in range(10):
		(d, vid) = results.get()
		print (searchHandler.distQuality(d), vid)
	#"""
