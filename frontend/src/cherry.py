import cherrypy
import pymongo
import shutil
import os
import shutil
import argparse
import re
import time
import datetime
import threading

from bson.objectid import ObjectId
from sys import stdout, stderr
from time import time

import indexing as idx
import kmeanstree as tree
import processhandler as ph

# instanciate and configure an argument parser
PARSER = argparse.ArgumentParser(description='Starts a CherryPy Webserver, for the find.vid project.')
PARSER.add_argument('port', metavar='PORT',
	help='The port on which the webserver will run')
PARSER.add_argument('database', metavar='DB',
	help='The name of the MongoDB Database on localhost')
PARSER.add_argument('collection', metavar='COLLECTION',
	help='The name of the Collection in the Database')
PARSER.add_argument('filename', metavar='FILENAME',
	help='The filename where the searchtree will be saved')
PARSER.add_argument("--quiet", action="store_true",
	help="No output will be created.")
PARSER.add_argument("--forcerebuild", action="store_true",
	help="Rebuild the searchtree and delete existing tree files if necessary.")

# parse input arguments
ARGS = PARSER.parse_args()

PORT = ARGS.port
DBNAME = ARGS.database
COLNAME = ARGS.collection
FEATUREWEIGHT = 0.5
KSPLIT = 32
KMAX = 8
FILENAME = ARGS.filename

# Directory of this file
ROOTDIR = os.path.abspath('.')

# Directory of HTML-Templates
HTMLDIR = os.path.join(ROOTDIR, 'html')

# Establish MongoDb Connection and get db and video collection
MONGOCLIENT = pymongo.MongoClient(port=8099)
DB = MONGOCLIENT[DBNAME]
VIDEOS = DB[COLNAME]
INDEXES = DB["indexes"]
HISTORY = DB["history"][COLNAME]

# Get config from MongoDb
CONFIG = VIDEOS.find_one({'_id': 'config'})

# Directories for Videos and Thumbnails (configured in CONFIG)
VIDEODIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['videopath']))
THUMBNAILDIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['thumbnailpath']))

# Directory for uploads
UPLOADDIR = os.path.abspath(os.path.join(VIDEODIR, 'uploads'))

# Multithreading
HANDLER = ph.ProcessHandler(maxProcesses=7, maxPrioritys=4)

STORETREE = os.path.join(CONFIG["abspath"], FILENAME)

SHADOWLOCK = threading.Lock()

def logInfo(message):
	stdout.write("INFO: %s\n" % str(message))

def logError(message):
	stderr.write("ERROR: %s\n" % str(message))

# Root of the whole CherryPy Server
class Root(object):
	filterChecked = True
	# Searchtree Object
	TREE = None

	def __init__(self):
		# Build tree; CURRENTLY DONE IN MAIN
		#self.TREE = tree.SearchHandler(videos=VIDEOS, name=STORETREE, featureWeight=FEATUREWEIGHT, processHandler=HANDLER)
		#self.TREE.loadOrBuildTree(k=KSPLIT, imax=KMAX, forceRebuild=(ARGS.forcerebuild)) 
		
		# Restart index processes in journal
		cursor = INDEXES.find()
		for proc in cursor:
			if proc["type"] == "Transkodieren":
				HANDLER.runTask(priority=1, onComplete=self.indexAndTranscodeComplete, target=self.transcodeAndIndexUpload, args=(proc["src"], proc["dst"], proc["searchable"], proc["filename"], proc["_id"]), kwargs={'restarted' : True},name=proc["_id"], onCompleteArgs=(proc["src"], proc["dst"], proc["_id"]))
			else: # "Indizieren"
				HANDLER.runTask(priority=0, onComplete=self.indexComplete, target=self.indexUpload, args=(proc["searchable"], proc["filename"], proc["_id"]), kwargs={'restarted' : True},name=proc["_id"], onCompleteArgs=tuple([proc["_id"]]))
			logInfo("Restarting process " + proc["_id"] + " from journal")
		

	# Returns the startpage, where the history is shown
	@cherrypy.expose
	def index(self):
		historyVideos = HISTORY.find({}, {'_id': 1, 'vidid': 1, 'sceneid': 1}).limit(50)

		content = "<h1>History</h1><br />"

		if historyVideos.count() == 0:
			content += "No Videos in the history."

		for video in historyVideos:
			if not video['vidid']:
				continue

			dbEntry = VIDEOS.find_one({'_id': video['vidid']}, {'scenes': 0})

			vidConfig = self.configScene(dbEntry, int(video['sceneid']))
			vidConfig.update({'historylink': video['_id']})

			content+=self.renderTemplate('history.html', vidConfig)

		config = {
			'title': 'Main',
			'searchterm': '',
			'content': content
		}
		return self.renderMainTemplate(config)

	@cherrypy.expose
	def history(self, historyid):
		historyEntry = HISTORY.find_one({'_id': ObjectId(historyid)})

		if not historyEntry:
			raise cherrypy.HTTPRedirect('/')

		similarScenes = historyEntry['similarScenes']

		content = ""
		if not similarScenes:
			content = 'No Scenes found for your search query.'
		else:
			scenes = []
			for similarScene in similarScenes:
				if similarScene == None:
					continue

				distance = similarScene[0]
				similarVidid = similarScene[1][0]
				similarSceneid = similarScene[1][1]

				similarVideo = VIDEOS.find_one({'_id': similarVidid}, {"scenes" : 0})

				simPercent = int(self.TREE.distQuality(distance) * 100)

				sceneConfig = self.configScene(similarVideo, similarSceneid)
				sceneConfig.update ({
					'hue': str(self.calcHue(simPercent)),
					'value': str(simPercent)
				})
				content += self.renderTemplate('similarscene.html', sceneConfig)

		config = {
			'title': 'Main',
			'searchterm': '',
			'content': content
		}
		return self.renderMainTemplate(config)

	# Renders a template.
	# filename - The filename of the template in HTMLDIR
	# config - A dictionary of all placeholders with their values
	def renderTemplate(self, filename, config):
		tplfile = open(os.path.join(HTMLDIR, filename)).read()

		# Replace each placeholder with the information in config
		for key, value in config.items():
			tplfile = re.sub(re.escape('<!--###'+key.upper()+'###-->'), str(value), tplfile)

		return tplfile

	# Calculates HSL value for similarity label color
	def calcHue(self, distance):
		value = int(distance)
		hsl = 120

		# Calculate HUE Value between 0 and 120
		hsl = value * 1.2

		return hsl

	# Renders the main template (template.html)
	# It sets the config for the uploadwindow
	# config - A dictionary of all placeholders with their values
	def renderMainTemplate(self, config):
		# Get the uploads
		uploads = self.getUploads()

		filterText = ""
		if self.filterChecked:
			filterText = "checked"

		# Expand config with uploads
		config.update({
			'filter': filterText,
			'videocount': uploads['videocount'],
			'scenecount': uploads['scenecount'],
			'uploads': uploads['uploads']
		})

		# Render the main template
		return self.renderTemplate('template.html', config)

	# Formats a time in hh:mm:ss
	# frame - The framenumber for which the time should be calculated
	# fps - The frames per seconds which will be used for calculation
	def formatTime(self, frame, fps):
		lengthInSec = int(frame/fps)
		seconds = lengthInSec % 60
		minutes = int(lengthInSec / 60) % 60
		hours = int(lengthInSec / 60 / 60) % 60

		return '%1.2d' % hours + ':' + '%1.2d' % minutes + ':' + '%1.2d' % seconds

	# Returns the configuration for a given video
	def configVideo(self, video):
		filename = str(video['filename'])
		videopath = os.path.join('/videos/', filename)

		fps = int(video['fps'])
		vidid = str(video['_id'])

		return {
			'url': videopath,
			'extension': os.path.splitext(filename)[1][1:],
			# TODO use the relative thumbnails path and confirm that this is the right way to do this
			'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(vidid))[0], 'scene0.jpeg'),
			'videoid': vidid,
			'deletelink': '/removeVideo?vidid='+vidid,
			'filename': os.path.basename(filename),
			'time': '0',
			'length': self.formatTime(int(video['cuts'][-1]), fps)
		}
	
	# Returns configuration for an indexing process
	def configIndexProc(self, indproc):
		# Basically just remaps _id to videohash...
		return {
			'FILENAME': indproc["filename"],
			'TIMESTAMP': datetime.datetime.fromtimestamp(indproc["timestamp"]).strftime('%d.%m.%Y %H:%M:%S'),
			'VIDEOHASH': indproc["_id"],
			'PROCESSTYPE' : indproc["type"]
		}


	# Returns the configuration for a given scene
	def configScene(self, video, sceneid):
		filename = video['filename']
		vidid = video['_id']
		fps = video['fps']
		cuts = video['cuts']

		videopath = os.path.join('/videos/', filename)

		filename = os.path.basename(filename)

		return {
			'url': videopath,
			'extension': os.path.splitext(filename)[1][1:],
			'time': str(cuts[sceneid] / fps),
			# TODO use the relative thumbnails path and confirm that this is the right way to do this
			'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(vidid))[0], 'scene'+str(sceneid)+'.jpeg'),
			'videoid': video['_id'],
			'scenecount': str(sceneid),
			'starttime': self.formatTime(int(cuts[sceneid]), fps),
			'filename': filename,
			'endtime': self.formatTime(int(cuts[sceneid+1]), fps)
		}

	# Fetches all uploads from the database (upload = True)
	# Returns a dictionary with {scenecount, videocount, uploads} 
	def getUploads(self):
		# Fetch all entries in video-collection where upload = True, except config
		# Sorted by Timestamp, only the 8 newest Videos
		uploadsFromDb = VIDEOS.find({'upload': True, 'removed':{'$not':{'$eq': True}}},{'scenes':0}).sort([('uploadtime', -1)]).limit(8)

		uploads = ""

		videocount = 0
		scenecount = 0

		for upload in uploadsFromDb:
			videocount += 1

			fps = int(upload['fps'])
			filename = os.path.basename(str(upload['filename']))
			scenes = len(upload['cuts']) - 1 # There are n scenes and n+1 cuts!
			
			scenecount += scenes

			vidid = str(upload['_id'])

			uploadconfig = {
				# TODO use the relative thumbnails path and confirm that this is the right way to do this
				'thumbnail': os.path.join('/thumbnails/', os.path.basename(vidid), 'scene0.jpeg'),
				'videoid': vidid,
				'deletelink': '/removeVideo?vidid='+vidid,
				'scenecount': scenes,
				'filename': filename,
				'length': self.formatTime(int(upload['cuts'][-1]), fps) # Last entry in cuts is also the framecount
			}
			
			uploads += self.renderTemplate('upload.html', uploadconfig)

		return {'scenecount': scenecount, 'videocount': videocount, 'uploads': uploads}

	# Returns a list of all currently running indexing processes
	@cherrypy.expose
	def indexes(self, vidId = None):
		content = ""
		cursorIndexingProcesses = INDEXES.find()

		# if a video ID has been passed, abort the process
		if vidId:
			print "Abort indexing process for video " , vidId
			INDEXES.remove({"_id": vidId})
			# INDEXPROCS[vidId].stop() or whatever
			# Cleanup is done by callbacks if they receive an error-marker as result
			HANDLER.stopProcess(name=vidId)
			raise cherrypy.HTTPRedirect('/indexes')

		if cursorIndexingProcesses.count() == 0:
			content = "There are no videos indexing at the moment."

		for indexProcess in cursorIndexingProcesses:
			content += self.renderTemplate('indexes.html', self.configIndexProc(indexProcess))

		config = {
		'title': 'Currently Indexing',
			'searchterm': '',
			'content': content
		}

		return self.renderMainTemplate(config)

	

	# Returns a list of videos, found by given name (GET parameter)
	# name - string after which is searched
	@cherrypy.expose
	def search(self, name = None):
		# If name is unspecified, redirect to startpage
		if not name:
			raise cherrypy.HTTPRedirect('/')

		# Get all videos with substring: <name> 
		videosFromDb = VIDEOS.find({"filename": { '$regex': name}, 'removed':{'$not':{'$eq': True}}}, {"scenes" : 0})

		# If no videos where found, tell the user
		if videosFromDb.count() == 0:
			content = 'No Videos found, for your search query: "'+name+'".'
		else:
			videos = []

			content = ""
			limit = 100
			counter = 1
			for video in videosFromDb:			
				content += self.renderTemplate('video.html', self.configVideo(video))
				
				if counter == limit:
					break

				counter+=1
			
		config = {
			'title': 'Search',
			'searchterm': name,
			'content': content
		}

		return self.renderMainTemplate(config)

	# Returns a list of scenes, found by similarscene search
	# vidid - ID of the source video
	# second - Second of the source scene in the source video
	@cherrypy.expose
	def searchScene(self, vidid = None, second = None):
		# If one of the parameters are unspecified, redirect to startpage
		if not vidid or not second:
			raise cherrypy.HTTPRedirect('/')

		# Get the scene where the frame is from TODO: Think of a more efficient way to do this
		video = VIDEOS.find_one({'_id': str(vidid), 'removed':{'$not':{'$eq': True}}}, {'scenes' : 0})
		if video == None:
			content = "The source video dosen't exist (anymore)."
		else:
			fps = int(video['fps'])
			second = float(second)
			frame = int(fps*second)

			sceneid = 0
		
			for i,endframe in enumerate(video['cuts']):
				if frame < endframe:
					sceneid = i-1
					break

			similarScenes = self.TREE.search(vidHash=vidid, sceneId=sceneid, wantedNNs=100, maxTouches=10000, filterChecked=self.filterChecked)

			HISTORY.insert({'timestamp': time(), 'vidid': vidid, 'sceneid': sceneid, 'similarScenes': similarScenes})

			content = ""
			if not similarScenes:
				content = 'No Scenes found for your search query.'
			else:
				scenes = []
				for similarScene in similarScenes:
					if similarScene == None:
						continue

					distance = similarScene[0]
					similarVidid = similarScene[1][0]
					similarSceneid = similarScene[1][1]

					similarVideo = VIDEOS.find_one({'_id': similarVidid}, {"scenes" : 0})

					simPercent = int(self.TREE.distQuality(distance) * 100)

					sceneConfig = self.configScene(similarVideo, similarSceneid)
					sceneConfig.update ({
						'hue': str(self.calcHue(simPercent)),
						'value': str(simPercent)
					})
					content += self.renderTemplate('similarscene.html', sceneConfig)

		config = {
			'title': 'Found Scenes',
			'searchterm': '',
			'content': content
		}

		return self.renderMainTemplate(config)

	# Returns a text-version of scenes, found by similarscene search
	# This function is for benchmark purposes
	# vidid - ID of the source video
	# frame - Framenumber of the source scene in the source video
	@cherrypy.expose
	def searchSceneList(self, vidid=None, frame=None, limit=100, nnlimit=1000):
		# If one of the parameters are unspecified, redirect to startpage
		if not vidid:
			return 'ERROR! - No vidid.'

		if not frame:
			return 'ERROR! - No framenumber.'

		# Get the scene where the frame is from TODO: Think of a more efficient way to do this
		video = VIDEOS.find_one({'_id': str(vidid), 'removed':{'$not':{'$eq': True}}}, {'scenes' : 0})

		sceneid = 0
		for i,endframe in enumerate(video['cuts']):
			if frame < endframe:
				sceneid = i-1
				break

		similarScenes = self.TREE.search(vidHash=vidid, sceneId=sceneid, wantedNNs=int(limit), maxTouches=int(nnlimit), filterChecked=True)

		result = ""

		if not similarScenes:
			return 'No Scenes found for your search query.'
		else:
			scenes = []
			for similarScene in similarScenes:
				if similarScene == None:
					continue

				similarVidid = similarScene[1][0]
				similarSceneid = similarScene[1][1]

				similarVideo = VIDEOS.find_one({'_id': similarVidid}, {"scenes" : 0})

				result += " " + similarVideo['filename'] + " " + str( int(similarVideo['cuts'][similarSceneid]) ) + " " + str( int(similarVideo['cuts'][similarSceneid+1])-1 ) + "\n" 

		return result

	# Returns all scenes for the given video, plus the originvideo
	# vidid - ID of the originvideo
	@cherrypy.expose
	def video(self, vidid = None):
		# If video is unspecified, redirect to startpage
		if not vidid:
			raise cherrypy.HTTPRedirect('/')

		videoFromDb = VIDEOS.find_one({'_id': str(vidid), 'removed':{'$not':{'$eq': True}}}, {"scenes" : 0})

		# If there is no video with the given vidid, redirect to startpage
		if not videoFromDb:
			raise cherrypy.HTTPRedirect('/')

		scenes = []
		
		# There is one scene less than cuts
		for sceneid in range(len(videoFromDb['cuts'])-1):
			scenes.append(self.renderTemplate('scene.html', self.configScene(videoFromDb, sceneid)))

		# Wrap the videos in "scene-wrap" div
		content = '<div class="scene-wrap">'
		for scene in scenes:
			content += scene

		content += "</div>"

		content += self.renderTemplate('originvideo.html', self.configVideo(videoFromDb))

		config = {
			'title': 'Scenes',
			'searchterm': '',
			'content': content
		}

		return self.renderMainTemplate(config)

	@cherrypy.expose
	def removeVideo(self, vidid):
		# If video is unspecified, redirect to startpage
		if not vidid:
			raise cherrypy.HTTPRedirect('/')
		
		self.TREE.deleteVideo(vidid)
		VIDEOS.update({'_id': vidid}, {'$set': {'removed': True}})
		
		raise cherrypy.HTTPRedirect('/')

	@cherrypy.expose
	def shadowTree(self):
		print "Try to Shadow Tree"
		
		SHADOWLOCK.acquire()
		try:
			if self.TREE.shadowCopy == None:
				self.TREE.shadowCopy = tree.SearchHandler(videos=VIDEOS, name=STORETREE + "_" + str(int(time())), featureWeight=FEATUREWEIGHT, processHandler=HANDLER)
			else:
				return
		finally:
			SHADOWLOCK.release()
		
		self.TREE.shadowCopy.loadOrBuildTree(k=KSPLIT, imax=KMAX, forceRebuild=True)

		self.TREE = self.TREE.shadowCopy
		logInfo("Tree was built and swapped!")

	# Uploads a video to the server, writes it to database and start processing
	# This function is intended to be called by javascript only.
	@cherrypy.expose
	def upload(self, searchable):
		cherrypy.response.timeout = 1000000

		allowedExtensions = [".avi", ".mp4", ".mpg", ".mkv", ".flv", ".webm", ".mov"]

		if bool(searchable):
			priority = 0
		else:
			priority = 2

		filename = os.path.basename(cherrypy.request.headers['x-filename'])
		basename = os.path.splitext(filename)[0]
		extension = os.path.splitext(filename)[1]

		if not extension in allowedExtensions:
			logError("Filetype '%s' is not within allowed extensions!" % extension)
			return "ERROR: Wrong file extension."

		destination = os.path.join(UPLOADDIR, filename)

		i = 2
		while os.path.exists(destination) or os.path.exists(os.path.splitext(destination)[0] + '.mp4'):
			destination = os.path.join(UPLOADDIR, basename + "_" + "%1.2d" % i + extension)
			logInfo('File already exists, renaming to %s!' % destination)

			i+=1

		basename = os.path.splitext(os.path.basename(destination))[0]
		
		with open(destination, 'wb') as f:
			shutil.copyfileobj(cherrypy.request.body, f)
		
		vidHash = idx.hashFile(destination, 65536)

		if extension != '.mp4':
			newdestination = os.path.join(UPLOADDIR, basename + ".mp4")
			filename = os.path.basename(newdestination)
			HANDLER.runTask(priority=priority, onComplete=self.indexAndTranscodeComplete, target=self.transcodeAndIndexUpload, args=(destination, newdestination, searchable, filename, vidHash),name=vidHash, onCompleteArgs=(destination, newdestination, vidHash))
		else:
			HANDLER.runTask(priority=priority, onComplete=self.indexComplete, target=self.indexUpload, args=(searchable, filename, vidHash),name=vidHash, onCompleteArgs=tuple([vidHash]))

	def transcodeAndIndexUpload(self, source, destination, searchable, filename, vidHash, restarted = False):
		logInfo("Transcoding Video to mp4 - '%s'" % filename)
	
		if bool(searchable):
			priority = 0
		else:
			priority = 2

		#Create an entry in "indexes" collection
		t = time()
		if not restarted:
			#Create an entry in "indexes" collection
			index = {}
			index["_id"] = vidHash
			index["timestamp"] = t
			index["filename"] = filename
			index["src"] = source
			index["dst"] = destination
			index["searchable"] = searchable
			index["type"] = "Transkodieren"
			INDEXES.insert(index)

		r = idx.transcode_video(source, destination, quiet=True)

		if r != 0:
			logError("Transcoding of video '%s' has failed" % filename)



		#Remove the entry to mark this indexing process as done
		INDEXES.remove({"_id" : vidHash, "timestamp" : t, "filename" : filename, "type" : "Transkodieren"})

		logInfo("Transcoding finished - '%s'" % filename)

		#if source != destination:
		#	os.remove(destination)

		HANDLER.runTask(priority=0, onComplete=self.indexComplete, target=self.indexUpload, args=(searchable, filename, vidHash), kwargs={'restarted' : restarted}, onCompleteArgs=tuple([vidHash]), name=vidHash)

	def indexUpload(self, searchable, filename, vidHash, restarted = False):
		logInfo("Indexing Video - '%s'" % filename)
		
		t = time()
		if not restarted:
			#Create an entry in "indexes" collection
			index = {}
			index["_id"] = vidHash
			index["timestamp"] = t
			index["filename"] = filename
			index["searchable"] = searchable
			index["type"] = "Indizieren"
			INDEXES.insert(index)

		vidid = idx.index_video(DBNAME, COLNAME, vidHash, os.path.join('uploads/', filename), searchable=bool(int(searchable)), uploaded=True, thumbpath=THUMBNAILDIR)

		#Remove the entry to mark this indexing process as done
		INDEXES.remove({"_id" : vidHash})

		logInfo("Indexing finished - '%s', removed process '%s' from journal" % (filename, vidHash))
		return vidid

	def indexAndTranscodeComplete(self, res, sourcefile, targetfile, vidHash):
		#vidid might be an error-object generated by the processhandler
		#in this case, we have to:
		#	delete the source video, in case transcoding was in process
		#	delete database entry with _id = vidid
		# 	recursively delete thumbnails/<vidid>
		# For processes that directly indexed, indexComplete is registered as callback

		# delete source video
		if os.path.exists(sourcefile): #Merely a defensive mechanism, should be always true
			os.remove(sourcefile)

		# process was killed by user, remove the targetfile aswell
		if res == False and os.path.exists(targetfile) and targetfile != sourcefile:
			os.remove(targetfile)

		# Hack to remove transcodings from the journal for sure
		INDEXES.remove({"_id" : vidHash})
		return self.indexComplete(res, vidHash)

	def indexComplete(self, res, vidHash):
		# process died, delete thumbnails folder if it exists and 
		if res == False:
			if os.path.exists(os.path.join(THUMBNAILDIR, vidHash)):
				shutil.rmtree(os.path.join(THUMBNAILDIR, vidHash))
			logInfo("Video indexing aborted. VideoID: %s" % vidHash)
	
		elif res == None:
			# TODO: error messages
			logError("File already exists.")
			return False
		else:
			self.TREE.addVideo(vidHash=vidHash)
			logInfo("Video successfully completed. VideoID: %s" % vidHash)
			return True

	@cherrypy.expose
	def toggleFilter(self):
		self.filterChecked = not self.filterChecked
		
		raise cherrypy.HTTPRedirect('/')

def killProcesses():
	HANDLER.nukeEverything()
	cherrypy.engine.exit()

if __name__ == '__main__':

	cherrypy.config.update({
		'server.socket_host': '0.0.0.0',
		'server.socket_port': int(PORT)
	})

	if ARGS.quiet:
		cherrypy.config.update({'environment': 'embedded'})

	# Mount the directories which are configured
	conf = {
		'/js': {
			'tools.staticdir.on': True,
			'tools.staticdir.dir': os.path.join(ROOTDIR, 'js')
		},
		'/css': {
			'tools.staticdir.on': True,
			'tools.staticdir.dir': os.path.join(ROOTDIR, 'css')
		},
		'/images': {
			'tools.staticdir.on': True,
			'tools.staticdir.dir': os.path.join(ROOTDIR, 'images')
		},
		'/thumbnails': {
			'tools.staticdir.on': True,
			'tools.staticdir.dir': THUMBNAILDIR
		},
		'/videos': {
			'tools.staticdir.on': True,
			'tools.staticdir.dir': VIDEODIR
		}
	}

	root = Root()
	cherrypy.tree.mount(root, '/', conf)

	files = os.listdir(CONFIG['abspath'])

	files = sorted(files)

	treefiles = []
	for name in files:
		if name.startswith(FILENAME):
			treefiles.append(name)

	if len(treefiles) == 0:
		treename = os.path.join(CONFIG['abspath'], FILENAME + "_" + str(int(time())))
	else:
		treename = os.path.join(CONFIG['abspath'], FILENAME + "_" + treefiles[-1].split('_')[-2])

	# Build Searchtree
	root.TREE = tree.SearchHandler(videos=VIDEOS, name=treename, featureWeight=FEATUREWEIGHT, processHandler=HANDLER)
	root.TREE.loadOrBuildTree(k=KSPLIT, imax=KMAX, forceRebuild=(ARGS.forcerebuild)) 


	# Set body size to 0 (unlimited), cause the uploaded files could be really big
	cherrypy.server.max_request_body_size = 0
	cherrypy.server.socket_timeout = 3600

	if hasattr(cherrypy.engine, 'block'):
		# 3.1 syntax
		if hasattr(cherrypy.engine, 'signal_handler'):
			cherrypy.engine.signal_handler.unsubscribe()
			cherrypy.engine.signal_handler.set_handler('SIGTERM', killProcesses)
			cherrypy.engine.signal_handler.set_handler('SIGINT', killProcesses)
			cherrypy.engine.signal_handler.subscribe()

		cherrypy.engine.start()
		cherrypy.engine.block()
	else:
		# 3.0 syntax
		cherrypy.server.quickstart()
		cherrypy.engine.start()
