import cherrypy
import pymongo
import shutil
import os
import re

# Indexer
import indexing as idx

# Directory of this file
ROOTDIR = os.path.abspath('.')

# Directory of HTML-Templates
HTMLDIR = os.path.join(ROOTDIR, 'html')

# Establish MongoDb Connection and get db and video collection
MONGOCLIENT = pymongo.MongoClient()
DB = MONGOCLIENT['findvid']
VIDEOS = DB['videos']

# Get config from MongoDb
CONFIG = VIDEOS.find_one({'_id': 'config'})

# Directories for Videos and Thumbnails (configured in CONFIG)
VIDEODIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['videopath']))
THUMBNAILDIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['thumbnailpath']))

# Directory for uploads
UPLOADDIR = os.path.abspath(os.path.join(VIDEODIR, 'uploads'))


# Renders a template.
# filename - The filename of the template in HTMLDIR
# config - A dictionary of all placeholders with their values
def renderTemplate(filename, config):
	tplfile = open(os.path.join(HTMLDIR, filename)).read()

	# Replace each placeholder with the information in config
	for key, value in config.items():
		tplfile = re.sub(re.escape('<!--###'+key.upper()+'###-->'), str(value), tplfile)

	return tplfile

# Renders the main template (template.html)
# It sets the config for the uploadwindow
# config - A dictionary of all placeholders with their values
def renderMainTemplate(config):
	# Get the uploads
	uploads = getUploads()

	# Expand config with uploads
	config.update({
		'videocount': uploads['videocount'],
		'scenecount': uploads['scenecount'],
		'uploads': uploads['uploads']
	})

	# Render the main template
	return renderTemplate('template.html', config)

# Formats a time in hh:mm:ss
# frame - The framenumber for which the time should be calculated
# fps - The frames per seconds which will be used for calculation
def formatTime(frame, fps):
	lengthInSec = int(frame/fps)
	seconds = lengthInSec % 60
	minutes = int(lengthInSec / 60) % 60
	hours = int(lengthInSec / 60 / 60) % 60

	return '%1.2d' % hours + ':' + '%1.2d' % minutes + ':' + '%1.2d' % seconds

# Returns the configuration for a given video
def configVideo(video):
	fps = int(video['fps'])
	filename = str(video['filename'])
	vidid = str(video['_id'])

	return {
		'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(filename))[0], 'scene0.jpeg'),
		'videoid': vidid,
		'filename': filename,
		'length': formatTime(int(video['framecount']), fps)
	}

# Returns the configuration for a given scene
def configScene(scene, filename, fps):
	videopath = os.path.join('/videos/', filename)

	return {
		'url': videopath,
		'extension': os.path.splitext(filename)[1][1:],
		'time': str(scene['startframe'] / fps),
		'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(filename))[0], 'scene'+str(scene['_id'])+'.jpeg'),
		'filename': filename,
		'scenecount': str(int(scene['_id'])),
		'starttime': formatTime(int(scene['startframe']), fps),
		'endtime': formatTime(int(scene['endframe']), fps)
	}

# Fetches all uploads from the database (upload = True)
# Returns a dictionary with {scenecount, videocount, uploads} 
def getUploads():
	# Fetch all entries in video-collection where upload = True, except config
	uploadsFromDb = VIDEOS.find(
		{
			'_id': 
				{ '$not': 
					{ '$eq': 'config' } 
				},
			'upload': True
		}
	)

	uploads = []

	videocount = 0
	scenecount = 0

	for upload in uploadsFromDb:
		progress = "100%" # TODO

		videocount += 1

		fps = int(upload['fps'])
		filename = str(upload['filename'])
		scenes = len(upload['scenes'])
		
		scenecount += scenes

		vidid = str(upload['_id'])

		uploadconfig = {
			'progress': progress,
			'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(filename))[0], 'scene0.jpeg'),
			'videoid': vidid,
			'scenecount': scenes,
			'filename': filename,
			'length': formatTime(int(upload['framecount']), fps)
		}
		
		uploads.append(renderTemplate('upload.html', uploadconfig))

	uploadsContent = ""
	for upload in uploads:
		uploadsContent += upload

	return {'scenecount': scenecount, 'videocount': videocount, 'uploads': uploadsContent}

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
		return renderMainTemplate(config)

	# Returns a list of videos, found by given name (GET parameter)
	# name - string after which is searched
	@cherrypy.expose
	def search(self, name = None):
		# If name is unspecified, redirect to startpage
		if not name:
			raise cherrypy.HTTPRedirect('/')

		# Get all videos with substring: <name> 
		videosFromDb = VIDEOS.find({"filename": { '$regex': name} })

		# If no videos where found, tell the user
		if videosFromDb.count() == 0:
			content = 'No Videos found, for your search query: "'+name+'".'
		else:
			videos = []

			for video in videosFromDb:			
				videos.append(renderTemplate('video.html', configVideo(video)))

			content = ""
			for video in videos:
				content += video

		config = {
			'title': 'Search',
			'searchterm': name,
			'content': content
		}

		return renderMainTemplate(config)

	# Returns a list of scenes, found by similarscene search
	# vidid - ID of the source video
	# frame - Framenumber of the source scene in the source video
	@cherrypy.expose
	def searchScene(self, vidid = None, frame = None):
		# If one of the parameters are unspecified, redirect to startpage
		if not vidid or not frame:
			raise cherrypy.HTTPRedirect('/')
		
		######## similar scene search is not yet implemented ########

		videoFromDb = VIDEOS.find_one({'_id': str(vidid)})

		if not videoFromDb:
			content = 'No Videos found, for your search query.'
		else:
			scenes = []
			
			filename = videoFromDb['filename']
			fps = videoFromDb['fps']

			for scene in videoFromDb['scenes']:
				scenes.append(renderTemplate('similarscene.html', configScene(scene, filename, fps)))

			content = ""
			for scene in scenes:
				content += scene

		config = {
			'title': 'Found Scenes',
			'searchterm': '',
			'content': content
		}

		return renderMainTemplate(config)

	# Returns all scenes for the given video, plus the originvideo
	# vidid - ID of the originvideo
	@cherrypy.expose
	def video(self, vidid = None):
		# If video is unspecified, redirect to startpage
		if not vidid:
			raise cherrypy.HTTPRedirect('/')

		videoFromDb = VIDEOS.find_one({'_id': str(vidid)})

		# If there is no video with the given vidid, redirect to startpage
		if not videoFromDb:
			raise cherrypy.HTTPRedirect('/')

		scenes = []
		
		filename = videoFromDb['filename']
		fps = videoFromDb['fps']

		for scene in videoFromDb['scenes']:
			scenes.append(renderTemplate('scene.html', configScene(scene, filename, fps)))

		# Wrap the videos in "scene-wrap" div
		content = '<div class="scene-wrap">'
		for scene in scenes:
			content += scene

		content += "</div>"

		videoFromDb = VIDEOS.find_one({'_id': str(vidid)})
		
		content += renderTemplate('originvideo.html', configVideo(videoFromDb))

		config = {
			'title': 'Scenes',
			'searchterm': '',
			'content': content
		}

		return renderMainTemplate(config)

	# Uploads a video to the server, writes it to database and start processing
	# This function is intended to be called by javascript only.
	@cherrypy.expose
	def upload(self):
		filename = os.path.basename(cherrypy.request.headers['x-filename'])
		destination = os.path.join(UPLOADDIR, filename)
		with open(destination, 'wb') as f:
			shutil.copyfileobj(cherrypy.request.body, f)

		if not idx.index_video(os.path.join(UPLOADDIR, filename), searchable=True, uploaded=True, thumbpath=THUMBNAILDIR):
			# TODO: error messages
			print "Error: File already exists"

if __name__ == '__main__':
	cherrypy.config.update('./global.conf')

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
