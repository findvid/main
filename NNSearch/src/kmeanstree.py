import numpy as npy
import random as rand
import sys
import Queue
import pickle
from pymongo import MongoClient

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
	"""
	def buildTree(self, data, k, maxiterations):
		# Output to get a feeling how long it takes (a long time)
		# 76 minutes for 1.000.000 entries with 1024 vectors
		if len(data) > 1000:
			print "len(data): ", len(data)
		# If there are less elements in data than a node should have children...
		if len(data) < k:
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
				if not cluster == []:
					center = npy.mean(npy.transpose([i[0] for i in cluster]), axis=1, dtype=npy.int)
					centersNew.append(center)

			# Check if the centers changed
			if npy.array_equal(centers, centersNew):
				centersFound = True

			centers = centersNew
		for center,cluster in zip(centers,clusters):
			# Create a child for each cluster
			child = KMeansTree(False, center, [])
			# Fill it with values
			child.buildTree(cluster, k, maxiterations)
			# Add chlid to children
			self.children.append(child)

	"""
	@param query		Feature array for the request. Find NNs to this one
	@param wantedNNs	Amount of NNs to be found
	@param maxPointsToTouch	Limit of leaves that get touched. Higher value -> better results but slower calculation
	"""
	def search(self, query, wantedNNs = 1, maxPointsToTouch = 42):
		# for the nodes that get checked later
		nextNodes = Queue.PriorityQueue()
		# for the results
		results = Queue.PriorityQueue()

		# search from root
		self.traverse(nextNodes, results, query)
		# while there are nextNodes and the max amount of points or the number of NNs is not reached
		while (not nextNodes.empty()) and (results.qsize() < maxPointsToTouch or results.qsize() < wantedNNs):
			# get the next clostest node
			_,nextNode = nextNodes.get()
			# and continue searching there
			nextNode.traverse(nextNodes, results, query)
		return results

	"""
	@param nextNodes	PrioQueue for 'checkout-later'-nodes
	@param results		PrioQueue for the results
	@param query		Feature array for the request. Find NNs to this one.
	"""
	def traverse(self, nextNodes, results, query):
		if self.isLeave:
			# put the leave to the results
			for center,value in self.children:
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
			closestChild.traverse(nextNodes, results, query)

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

# TODO For now set the type of feature here!
usedFeature = 'edges'

"""
Build a tree from a given database containing videodata

@param db	The database

@return		kmeans tree to search on
"""
def buildTreeFromDatabase(db):
	print "Reading data from database"
	videos = db["videos"]

	vids = videos.find({'_id':{'$not':{'$eq':'config'}}})

	data = []
	for vid in vids:
		scenes = vid['scenes']
		vidHash = vid['_id']
		for scene in scenes:
			# TODO better replaced with enumerate
			sceneId = scene['_id']
			feature = npy.array(scene[usedFeature])
			data.append((feature,(vidHash,int(sceneId))))

	print "Building Tree"
	tree = KMeansTree(False, [], [])
	tree.buildTree(data, 32, 15)
	return tree

"""
Search for a scene from a database

@param db		Database containing the query
@param tree		kmeans tree to search on
@param vidHash		id of the video of the query scene
@param sceneId		id of the query scene
@param wantedNNs	amount of NNs you want
@param maxTouches	how many leaves should be touched at max. currently not different to wantedNNs

@return			PrioriyQueue containing the results (>= wantedNNs if the tree is big enough
"""
def searchForScene(db, tree, vidHash, sceneId, wantedNNs, maxTouches):
	videos = db["videos"]
	vid = videos.find_one({'_id':vidHash})
	scene = vid['scenes'][sceneId]
	feature = npy.array(scene[usedFeature])
	return tree.search(feature, wantedNNs, maxTouches)

# Example code
"""
client = MongoClient()
db = client["findvid"]
videos = db["videos"]

vid = videos.find_one({'filename':{'$regex':'.*hardcuts\.mp4.*'}})

tree = buildTreeFromDatabase(db)

results = searchForScene(db, tree, vid['_id'], 0, 10, 10)

print results.get()
print results.get()
print results.get()
print results.get()
print results.get()
#"""

"""#
print "Building test data..."
data = []
for i in range(0, 10000):
	data.append((npy.random.randint(0, 1000000, 1024), "Data-" + str(i)))

print "Building tree..."
tree = KMeansTree(False, [], [])
tree.buildTree(data, 8, 10)

#" ""
print "Saving data..."
pickle.dump(data, open("data.p", "wb"))

print "Saving tree..."
pickle.dump(tree, open("tree.p", "wb"))
#" ""


print "Loading tree..."
data = pickle.load(open("data.p", "rb"))
print "Loading data..."
tree = pickle.load(open("tree.p", "rb"))
#" ""
query = npy.random.randint(0, 1000000, 1024)

#print "Tree: ", str(tree)
nns = tree.search(query, 5, 1000)

print "Q: ", query
for i in range(0, 5):
	nn = nns.get()
	print "NN #", i, ": ", nn

raw_input("\nPress key to start sanity check\n")

print "1 Tree search (5 NNs, 1000 Touches):"
nns = tree.search(query, 5, 1000)
print nns.get()
print nns.get()
print nns.get()
print nns.get()
print nns.get()

print "\n1 Linear search:"

closest = None
mindist = sys.maxint
for point,name in data:
	distance = dist(point, query)
	if distance < mindist:
		mindist = distance
		closest = (distance, name)

print closest
"""
