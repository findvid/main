import numpy as npy
import random as rand
import Queue
import pickle

class KMeansTree:
	isLeave = False
	center = []
	childs = []

	def __init__(self, isLeave, childs, center):
		self.isLeave = isLeave
		self.childs = childs
		self.center = center

	def __str__(self):
		return self.str2("")

	def str2(self,i):
		retval = i
		if self.isLeave:
			retval += "Leave:["
		else:
			retval += "Node:["
		retval += "Center:" + str(self.center) + "\n"
		if self.isLeave:
			for c in self.childs:
				retval += i + "     " + str(c) + "\n"
		else:
			for c in self.childs:
				retval += c.str2(i + "    ") + "\n"
		retval += i + "]"
		return retval

def dist(x, y):
	if len(x) != len(y):
		raise Exception("Can't calc distance for arrays of differnt length.");
	result = 0;
	for a,b in zip(x,y):
		result += (a-b)**2
	return result;

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

def buildTree(D,K,Imax,center):
	if len(D) < K:
		return KMeansTree(True, D, center)
	P = calg(D,K)

	conv = False
	iterations = 0
	while not conv and iterations < Imax:
		C = []
		for i,_ in enumerate(P):
			C.append([])
		for x,n in D:
			dis = 10**99
			index = 0;
			for i,p in enumerate(P):
				d = dist(p,x)
				if d < dis:
					dis = d
					index = i
			C[index].append((x,n))
		P_new = []
		for i,c in enumerate(C):
			# This if isn't quite correct...
			# It's just to prevent empty clusters causing an exception
			# TODO: Think about a better way
			if c == []:
				P_new.append(P[i])
				continue
			c = [i[0] for i in c]
			c = npy.transpose(c)
			tmp = []
			for cc in c:
				tmp.append(int(npy.mean(cc)))
			P_new.append(tmp)
		conv = True
		for p1,p2 in zip(P,P_new):
			if p1 != p2:
				conv = False
		P = P_new
		iterations += 1
	childs = []
	for i,c in enumerate(C):
		childs.append(buildTree(c, K, Imax, P[i]))
	return KMeansTree(False, childs, center)

def traverseKMeansTree(tree, PQ, R, count, query):
	if tree.isLeave:
		for c,n in tree.childs:
			R.put((dist(c,query),(c,n)))
		count += len(tree.childs)
	else:
		dis = 10**99
		index = 0
		for i,c in enumerate(tree.childs):
			d = dist(query,c.center)
			if d < dis:
				dis = d
				index = i
		for i,c in enumerate(tree.childs):
			if (i != index):
				PQ.put((dist(c.center,query),c))
		count = traverseKMeansTree(tree.childs[index], PQ, R, count, query)
	return count

def searchKMeansTree(tree, query, L, K):
	count = 0
	PQ = Queue.PriorityQueue()
	R = Queue.PriorityQueue()
	traverseKMeansTree(tree, PQ, R, count, query)
	while (not PQ.empty()) and count < L:
		_,N = PQ.get()
		count = traverseKMeansTree(N, PQ, R, count, query)
	return R

#data = [
#	([1,2,3],'Eins'),
#	([4,5,3],'Zwei'),
#	([123,423,123],'Drei'),
#	([23,423,22],'Vier'),
#	([23,23,124],'Fuenf'),
#	([123,23,125],'Sechs'),
#	([123,423,26],'Sieben')
#	]
data = []

print "Building test data..."
for x in range(0,400):
	for y in range(0,400):
		data.append(([x,y],"Data-" + str(x) + "-" + str(y)))

#print data

k = 16
i_max = 10

#print data
#print dist([0,0,0],[3,3,3])
#print calg(data,2)
print "Hang on. Building a tree..."
tree = buildTree(data,k,i_max,0)
pickle.dump(tree, open("testtree.p", "wb"))
#tree = pickle.load(open("testtree.p", "rb"))
#print tree
print "Let's try a search on it:"
R = searchKMeansTree(tree, [10,30], 6, 3);
print R.get();
print R.get();
print R.get();

raw_input("Press key to start sanity check")

print "Sanity check: 100 times linear search vs. 100 times KMeans search"
print "100 times searchKMeansTree:"

for i in range(1,100):
	R = searchKMeansTree(tree, [80,30], 6, 3);
print R.get();
print R.get();
print R.get();

print "100 times searchLinear:"

index = 0
for i in range(1,100):
	dis = 10**99
	q = [80,30]
	for i,(a,n) in enumerate(data):
		d = dist(q,a)
		if d < dis:
			dis = d
			index = i

print data[index]
