import pymongo
import shutil
import os
import argparse
import re
import urllib2
import subprocess
import time

import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

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
	help='The original path, where the queries and targets are (absolute!!!). This is used for the informations in the output files and reading the groundtruth file.')
PARSER.add_argument('tolerance', metavar='TOLERANCE',
	help='The tolerance (in frames) how many frames the found target can be away from ground truth')

# parse input arguments
ARGS = PARSER.parse_args()

SERVERFILE = ARGS.serverfile
PORT = ARGS.port
DBNAME = ARGS.database
COLNAME = ARGS.collection
ORIGPATH = ARGS.origpath
TOLERANCE = int(ARGS.tolerance)

# Directory of this file
ROOTDIR = os.path.abspath('.')

if (not os.path.exists(SERVERFILE)) or (not os.path.isfile(SERVERFILE)):
	print "The webserver file: '" + SERVERFILE + "', doesn't exist or is not a file!"
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

def startWebserver(ksplit, imax, dbfile):
	command = 'python ' + SERVERFILE + ' ' + PORT + ' ' + DBNAME + ' ' + COLNAME + ' ' + str(ksplit) + ' ' + str(imax) + ' ' + str(dbfile) + ' --quiet'
	return subprocess.Popen('exec ' + command, stdout=subprocess.PIPE, shell=True)

def stopWebserver(process):
	process.terminate()
	return process.wait()

def benchmarkTreeBuild(outdir='./out'):
	videos = VIDEOS.find({'_id': {'$not': {'$eq': 'config'}}}, {'scenes': 0})

	videocount = videos.count()
	scenecount = 0

	for video in videos:
		scenecount += len(video['cuts']) - 1

	print "Building the trees with " + str(videocount) + " Videos and " + str(scenecount) + " Scenes\n"
	
	for i in range(0, 2):

		paramValues = []
		timeValues = []
		hitValues = []
		searchTimeValues = []
		for power in range(0, 8):
			if i == 0:
				ksplit = 2**power
				imax = 100
			else:
				ksplit = 8
				imax = 2**power

			paramValues.append(power)

			print "Bulding tree - ksplit: " + str(ksplit) + "  -  imax: " + str(imax)

			starttime = time.time()
			kmeanstree.loadOrBuildAndSaveTree(videos=VIDEOS, filename=os.path.join(ROOTDIR, 'temptree.db'), k=ksplit, imax=imax)
			endtime = time.time()

			difftime = endtime-starttime
			print "Builded tree in " + str(difftime) + " seconds"

			timeValues.append(difftime)

			print "Testing SceneSearch..."
			process = startWebserver(ksplit=ksplit, imax=imax, dbfile=os.path.join(ROOTDIR, 'temptree.db'))
			searchBenchmark = sceneSearch(limit=10)
			stopWebserver(process)

			hitValues.append(searchBenchmark['correct'])
			searchTimeValues.append(searchBenchmark['averagetime'])

			print str(searchBenchmark['correct']) + "% hitrate"
			print "In average " + str(searchBenchmark['averagetime']) + " seconds per searchquery\n"

			os.remove(os.path.join(ROOTDIR, 'temptree.db'))

		if i == 0:
			label = "ksplit"
		else:
			label = "imax"

		plt.subplot(311)
		plt.title('Time of tree builds')
		plt.plot(paramValues, timeValues, 'ro')
		plt.xlabel(label)
		plt.ylabel('time in seconds')

		plt.subplot(312)
		plt.title('Hitrate of trees')
		plt.plot(paramValues, hitValues, 'ro')
		plt.xlabel(label)
		plt.ylabel('hits in %')

		plt.subplot(313)
		plt.title('Average time per scenesearch of trees')
		plt.plot(paramValues, searchTimeValues, 'ro')
		plt.xlabel(label)
		plt.ylabel('avrg. time in seconds')

		plt.tight_layout()
		plt.savefig(os.path.join(outdir, label + ".png"))

		plt.gcf().clear()

def benchmarkSceneSearch(outdir='./out'):
	#for i in range(0, 10):
	return True


# Search for all videos starting with "query" in database
# RegEx: Search for substring "query" in path, with digits after the string and a period after the digits
# (so we can be sure 'query' is not some directory name or else...)
def sceneSearch(limit=100, nnlimit=1000, outdir='./out'):
	queryvideos = VIDEOS.find({'filename': {'$regex': 'query\d*\.'}}, {'filename': 1, 'cuts': 1})

	count = queryvideos.count()

	totalTime = 0
	correct = 0
	for video in queryvideos:

		vidid = video['_id']
		filename = video['filename']
		scenecount = len(video['cuts']) - 1

		frame = 0
		# When the video has more than 1 scene, take the longest
		if scenecount > 1:
			length = 0
			for cut in range(0, scenecount):
				thislength = video['cuts'][cut+1]-1 - video['cuts'][cut]
				if thislength > length:
					length = thislength
					frame = video['cuts'][cut]

		response = False
		tries = 0
		while not response:
			try:
				starttime = time.time()
				response = urllib2.urlopen('http://localhost:'+str(PORT)+'/searchSceneList?vidid='+str(vidid)+'&frame='+str(frame)+'&limit='+str(limit)+'&nnlimit='+str(nnlimit))
				body = response.read()
				endtime = time.time()
			except urllib2.URLError:
				# Webserver is not ready - wait 10 seconds and try again
				time.sleep(10)
				response = False

		results = body.split('\n')
		for lineNum in range(0, len(results)):
			results[lineNum] = results[lineNum].split()

		gtline = None
		basename = os.path.splitext(os.path.basename(filename))[0]
		for query in GTDATA:
			# The last splitted entry can be empty
			if len(query) == 0:
				continue

			gtfilename = os.path.splitext(os.path.basename(query[0]))[0]
			if basename == gtfilename:
				gtline = query
				break

		if not gtline:
			print COLORFAIL + "ERROR: Video not found in Ground Truth! Skipping... ("+basename+")" + COLORNORMAL
			count-=1
			continue

		difftime = endtime-starttime
		totalTime += difftime

		counter = 1.0
		for result in results:
			# The last splitted entry can be empty
			if len(result) == 0:
				continue

			# Same target file
			if os.path.splitext(os.path.basename(result[0]))[0] == os.path.splitext(os.path.basename(gtline[1]))[0]:
				# Check if time values are within tolerance

				startResult = int(result[1])
				endResult = int(result[2])

				startGt = int(gtline[2])
				endGt = int(gtline[3])

				# If the start value is not in tolerance range of groundtruth value: continue
				if not ((startResult >= startGt-TOLERANCE) and (startResult <= startGt+TOLERANCE)):
					continue

				# If the end value is not in tolerance range of groundtruth value: continue
				if not ((endResult >= endGt-TOLERANCE) and (endResult <= endGt+TOLERANCE)):
					continue

				# If we got here, congratulation. We found a correct target.
				correct += counter
				break

			counter-=0.1
	

		#outfile.write(str(filename) + ' ' + str(difftime) + '\n')
		#outfile.write(body)

		#print "Searchquery took " + str(difftime) + " seconds\n"

	correctPerc = correct / float(count) * 100.0
	averagetime = totalTime / float(count)

	return {'correct': correctPerc, 'averagetime': averagetime}

	#outfile.close()
	
if __name__ == '__main__':
	outdir = os.path.join(ROOTDIR, 'out')
	try:
		os.mkdir(outdir)
	except OSError:
		print COLORWARNING + "WARNING: Output directory already exists. Existing data may be overwritten." + COLORNORMAL

	benchmarkTreeBuild(outdir)

