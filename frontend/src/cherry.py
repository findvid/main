import cherrypy
import pymongo
import shutil
import os
import argparse
import re

# Indexer
import indexing as idx

import kmeanstree as tree

# instanciate and configure an argument parser
PARSER = argparse.ArgumentParser(description='Starts a CherryPy Webserver, for the find.vid project.')
PARSER.add_argument('port', metavar='PORT',
	help='The port on which the webserver will run')
PARSER.add_argument('database', metavar='DB',
	help='The name of the MongoDB Database on localhost')
PARSER.add_argument('collection', metavar='COLLECTION',
	help='The name of the Collection in the Database')
PARSER.add_argument('ksplit', metavar='KSPLIT',
	help='The number of splits of the k-means-tree')
PARSER.add_argument('kmax', metavar='KMAX',
	help='The number of maximal children in the k-means-tree')
PARSER.add_argument('filename', metavar='FILENAME',
	help='The filename where the searchtree will be saved')
PARSER.add_argument("--quiet", action="store_true",
	help="No output will be created.")

# parse input arguments
ARGS = PARSER.parse_args()

PORT = ARGS.port
DBNAME = ARGS.database
COLNAME = ARGS.collection
KSPLIT = ARGS.ksplit
KMAX = ARGS.kmax
FILENAME = ARGS.filename

# Directory of this file
ROOTDIR = os.path.abspath('.')

# Directory of HTML-Templates
HTMLDIR = os.path.join(ROOTDIR, 'html')

# Establish MongoDb Connection and get db and video collection
MONGOCLIENT = pymongo.MongoClient(port=8099)
DB = MONGOCLIENT[DBNAME]
VIDEOS = DB[COLNAME]

# Get config from MongoDb
CONFIG = VIDEOS.find_one({'_id': 'config'})

# Directories for Videos and Thumbnails (configured in CONFIG)
VIDEODIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['videopath']))
THUMBNAILDIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['thumbnailpath']))

# Directory for uploads
UPLOADDIR = os.path.abspath(os.path.join(VIDEODIR, 'uploads'))

# Searchtree array
TREE = []

# Filename of saved tree
STORETREE = os.path.join(CONFIG['abspath'], FILENAME)

FILTER = True

# Root of the whole CherryPy Server
class Root(object):

	# Returns the startpage, where the history is shown
	@cherrypy.expose
	def index(self):
		
		content = """<div style="padding-left:5px"><b>Latest search terms:</b><br>...<br><br><b>Latest scene searches:</b><br>...</div>"""

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

		if value >= 50:
			hsl = (value - 50) * 2.4
		else:
			hsl = 0

		return hsl

	# Renders the main template (template.html)
	# It sets the config for the uploadwindow
	# config - A dictionary of all placeholders with their values
	def renderMainTemplate(self, config):
		# Get the uploads
		uploads = self.getUploads()

		# Expand config with uploads
		config.update({
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
			'filename': os.path.basename(filename),
			'length': self.formatTime(int(video['cuts'][-1]), fps)
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
		uploadsFromDb = VIDEOS.find({'upload': True},{'scenes':0}).sort([('uploadtime', -1)]).limit(8)

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
				'scenecount': scenes,
				'filename': filename,
				'length': self.formatTime(int(upload['cuts'][-1]), fps) # Last entry in cuts is also the framecount
			}
			
			uploads += self.renderTemplate('upload.html', uploadconfig)

		return {'scenecount': scenecount, 'videocount': videocount, 'uploads': uploads}

	# Returns a list of videos, found by given name (GET parameter)
	# name - string after which is searched
	@cherrypy.expose
	def search(self, name = None):
		# If name is unspecified, redirect to startpage
		if not name:
			raise cherrypy.HTTPRedirect('/')

		# Get all videos with substring: <name> 
		videosFromDb = VIDEOS.find({"filename": { '$regex': name}}, {"scenes" : 0})

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
		video = VIDEOS.find_one({'_id': str(vidid)}, {'scenes' : 0})
		fps = int(video['fps'])
		second = float(second)
		frame = int(fps*second)

		sceneid = 0
		
		for i,endframe in enumerate(video['cuts']):
			if frame < endframe:
				sceneid = i-1
				break

		similarScenes = tree.searchForScene(videos=VIDEOS, tree=TREE, vidHash=vidid, sceneId=sceneid, wantedNNs=1000, maxTouches=1000)

		content = ""
		if not similarScenes:
			content = 'No Scenes found for your search query.'
		else:
			scenes = []
			i = 0
			while (not similarScenes.empty()) and i < 100:
				similarScene = similarScenes.get()	

				if similarScene == None:
					continue

				similarVidid = similarScene[1][0]
				similarSceneid = similarScene[1][1]

				similarVideo = VIDEOS.find_one({'_id': similarVidid}, {"scenes" : 0})

				sceneConfig = self.configScene(similarVideo, similarSceneid)
				sceneConfig += {
					'hue': str(self.calcHue(100)),
					'value': str(100)
				}
				content += self.renderTemplate('similarscene.html', sceneConfig)

				i+=1

		config = {
			'title': 'Found Scenes',
			'searchterm': '',
			'content': content
		}

		return self.renderMainTemplate(config)

	# Returns a text-version of scenes, found by similarscene search
	# vidid - ID of the source video
	# frame - Framenumber of the source scene in the source video
	@cherrypy.expose
	def searchSceneList(self, vidid = None, frame = None):
		# If one of the parameters are unspecified, redirect to startpage
		if not vidid:
			return 'ERROR! - No vidid.'

		if not frame:
			return 'ERROR! - No framenumber.'

		# Get the scene where the frame is from TODO: Think of a more efficient way to do this
		video = VIDEOS.find_one({'_id': str(vidid)}, {'scenes' : 0})

		sceneid = 0
		for i,endframe in enumerate(video['cuts']):
			if frame < endframe:
				sceneid = i-1
				break

		similarScenes = tree.searchForScene(videos=VIDEOS, tree=TREE, vidHash=vidid, sceneId=sceneid, wantedNNs=1000, maxTouches=1000)

		result = ""

		if not similarScenes:
			return 'No Scenes found for your search query.'
		else:
			scenes = []
			i = 0
			while (not similarScenes.empty()) and i < 100:
				similarScene = similarScenes.get()	

				if similarScene == None:
					continue

				similarVidid = similarScene[1][0]
				similarSceneid = similarScene[1][1]

				similarVideo = VIDEOS.find_one({'_id': similarVidid}, {"scenes" : 0})

				result += " " + similarVideo['filename'] + " " + str(similarVideo['cuts'][similarSceneid]) + " " + str( int(similarVideo['cuts'][similarSceneid+1])-1 ) + "\n" 
				i+=1

		return result

	# Returns all scenes for the given video, plus the originvideo
	# vidid - ID of the originvideo
	@cherrypy.expose
	def video(self, vidid = None):
		# If video is unspecified, redirect to startpage
		if not vidid:
			raise cherrypy.HTTPRedirect('/')

		videoFromDb = VIDEOS.find_one({'_id': str(vidid)}, {"scenes" : 0})

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

	# Uploads a video to the server, writes it to database and start processing
	# This function is intended to be called by javascript only.
	@cherrypy.expose
	def upload(self):
		filename = os.path.basename(cherrypy.request.headers['x-filename'])
		destination = os.path.join(UPLOADDIR, filename)
		with open(destination, 'wb') as f:
			shutil.copyfileobj(cherrypy.request.body, f)

		vidid = idx.index_video(VIDEOS, os.path.join('uploads/', filename), searchable=True, uploaded=True, thumbpath=THUMBNAILDIR)
		if vidid == None:
			# TODO: error messages
			print "Error: File already exists."
			return "Error: File already exists."
		else:
			tree.addVideoDynamic(VIDEOS, vidid)
			return "File successfully uploaded."


	@cherrypy.expose
	def toggleFilter(self):
		FILTER = not FILTER
		print FILTER

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

	# Build Searchtree
	
	# TODO: Exception Handling
	TREE = tree.loadOrBuildAndSaveTree(videos=VIDEOS, filename=STORETREE, k=KSPLIT, imax=KMAX)

	cherrypy.tree.mount(Root(), '/', conf)

	# Set body size to 0 (unlimited), cause the uploaded files could be really big
	cherrypy.server.max_request_body_size = 0

	if hasattr(cherrypy.engine, 'block'):
		# 3.1 syntax
		cherrypy.engine.start()
		cherrypy.engine.block()
	else:
		# 3.0 syntax
		cherrypy.server.quickstart()
		cherrypy.engine.start()
