import pymongo
import shutil
import os
import argparse
import re
import urllib2
import subprocess
import time
import kmeanstree

from sys import exit

# instanciate and configure an argument parser
PARSER = argparse.ArgumentParser(description='')
PARSER.add_argument('serverfile', metavar='SERVERFILE',
	help='The full path to the cherrypy python file, which will be used for the benchmark (absolute!!!)')
PARSER.add_argument('port', metavar='PORT',
	help='The port on which the webserver will run')
PARSER.add_argument('database', metavar='DB',
	help='The name of the MongoDB Database on localhost')
PARSER.add_argument('collection', metavar='COLLECTION',
	help='The name of the Collection in the Database')
PARSER.add_argument('origpath', metavar='ORIGINALPATH',
	help='The original path, where the queries and targets are (absolute!!!). This is used for the informations in the output files.')
PARSER.add_argument('out', metavar='OUT',
	help='The output path (Each benchmark creates a folder and its files in it).')
PARSER.add_argument('tolerance', metavar='TOLERANCE',
	help='The tolerance (in %) how many frames the found target can be away from ground truth')

# parse input arguments
ARGS = PARSER.parse_args()

SERVERFILE = ARGS.serverfile
PORT = ARGS.port
DBNAME = ARGS.database
COLNAME = ARGS.collection
ORIGPATH = ARGS.origpath
OUTPATH = ARGS.out
TOLERANCE = ARGS.tolerance

if (not os.path.exists(SERVERFILE)) or (not os.path.isfile(SERVERFILE)):
	print "The webserver file: '" + SERVERFILE + "', doesn't exist or is not a file!"
	sys.exit(1)

if (not os.path.exists(OUTPATH)) or (not os.path.isdir(OUTPATH)):
	print "The given path: '" + OUTPATH + "', doesn't exist or is not a directory!"
	sys.exit(1)

if (not os.path.exists(ORIGPATH)) or (not os.path.isdir(ORIGPATH)):
	print "The given path: '" + ORIGPATH + "', doesn't exist or is not a directory!"
	sys.exit(1)

GTFILE = os.path.join(ORIGPATH, 'BENCHMARK_FULL.TXT')

if (not os.path.exists(GTFILE)) or (not os.path.isfile(GTFILE)):
	print "The groundtruth file: '" + GTFILE + "', doesn't exist or is not a file!"
	sys.exit(1)

# Read Ground Truth file and split on lines and on spaces for each line
# Ground Truth looks like:
# <querypath> <targetpath> <position_from> <position_to>...
GTDATA = open(GTFILE, 'r').read().split('\n')
for lineNum in range(0, len(GTDATA)):
	GTDATA[lineNum] = GTDATA[lineNum].split()

# Establish MongoDb Connection and get db and video collection
MONGOCLIENT = pymongo.MongoClient(port=8099)
DB = MONGOCLIENT[DBNAME]
VIDEOS = DB[COLNAME]

# Get config from MongoDb
CONFIG = VIDEOS.find_one({'_id': 'config'})

COLORNORMAL = '\033[0m'
COLORWARNING = '\033[93m'
COLORFAIL = '\033[91m'

def startWebserver(ksplit, kmax):
	command = 'python ' + SERVERFILE + ' ' + PORT + ' ' + DBNAME + ' ' + COLNAME + ' ' + str(ksplit) + ' ' + str(kmax) + ' searchtree_benchmark.db --quiet'
	return subprocess.Popen('exec ' + command, stdout=subprocess.PIPE, shell=True)

def stopWebserver(process):
	process.terminate()
	return process.wait()

def benchmarkTreeBuild():
	videos = VIDEOS.find({'_id': {'$not': {'$eq': 'config'}}}, {'scenes': 0})

	videocount = videos.count()
	scenecount = 0

	for video in videos:
		scenecount += len(video['cuts']) - 1

	print "Building the trees with " + str(videocount) + " Videos and " + str(scenecount) + " Scenes\n"
	
	for power in range(0, 8):	
		ksplit = 2**power
		kmax = 2**power
		print "Bulding tree - ksplit: " + str(ksplit) + "  -  kmax: " + str(kmax)

		starttime = time.time()
		kmeanstree.loadOrBuildAndSaveTree(videos=VIDEOS, filename='./temptree.db', k=ksplit, imax=kmax)
		endtime = time.time()

		os.remove('./temptree.db')

		difftime = endtime-starttime
		print "Builded tree in " + str(difftime) + " seconds\n"
		print "Testing SceneSearch"


	return True

# Search for all videos starting with "query" in database
# RegEx: Search for substring "query" in path, with digits after the string and a period after the digits
# (so we can be sure 'query' is not some directory name or else...)
def benchmarkSceneSearch(limit=100, nnlimit=1000):
	queryvideos = VIDEOS.find({'filename': {'$regex': 'query\d*\.'}}, {'filename': 1, 'cuts': 1})

	count = queryvideos.count()

	#print str(count) + " videos starting with 'query' where found in database.\n"
	#outfile = open(os.path.join(path, 'out.txt'), 'w')

	for video in queryvideos:

		vidid = video['_id']
		filename = video['filename']
		scenecount = len(video['cuts']) - 1
		
		#print "Processing video: '"+str(filename)+"' - '"+str(vidid)+"'"

		frame = 0
		if scenecount > 1:
			#print COLORWARNING + "WARNING! - The query-video has more than 1 scene. Using the longer one!" + COLORNORMAL
			length = 0
			for cut in range(0, scenecount):
				thislength = video['cuts'][cut+1]-1 - video['cuts'][cut]
				if thislength > length:
					length = thislength
					frame = video['cuts'][cut]

		response = False
		tries = 0
		while not response and tries < 3:
			try:
				starttime = time.time()
				response = urllib2.urlopen('http://localhost:'+str(PORT)+'/searchSceneList?vidid='+str(vidid)+'&frame='+str(frame)+'&limit='+limit+'&nnlimit='+nnlimit)
				body = response.read()
				endtime = time.time()
			except urllib2.URLError:
				#print COLORFAIL + "Webserver not reachable! Trying again in 10 seconds..." + COLORNORMAL
				time.sleep(10)
				response = False
			tries+=1
		
		if tries >= 3:
			print "\n" + COLORFAIL + "ERROR! - Webserver could not be reached after 3 retries." + COLORNORMAL
			sys.exit(1)

		difftime = endtime-starttime

		#outfile.write(str(filename) + ' ' + str(difftime) + '\n')
		#outfile.write(body)

		#print "Searchquery took " + str(difftime) + " seconds\n"

	outfile.close()
	
if __name__ == '__main__':
	
	#process = startWebserver(8, 100)
 
	testdir = os.path.join(OUTPATH, 'scenesearch')
	try:
		os.mkdir(testdir)
	except OSError:
		print COLORWARNING + "WARNING! - The output directory already contains files. Older benchmarks may be overwritten." + COLORNORMAL
	#benchmarkSceneSearch(testdir)

	#stopWebserver(process)
	
	print "Testing Tree Times..."
	benchmarkTreeBuild()