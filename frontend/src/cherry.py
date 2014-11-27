import cherrypy
import pymongo
import shutil
import os
import re
import cutter

ROOTDIR = os.path.abspath('.')

MONGOCLIENT = pymongo.MongoClient()
DB = MONGOCLIENT['findvid']
VIDEOS = DB['videos']

HTMLDIR = os.path.join(ROOTDIR, 'html')
DEBUG = True

CONFIG = VIDEOS.find_one({'_id': 'config'})

VIDEODIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['videopath']))
THUMBNAILDIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['thumbnailpath']))

UPLOADDIR = os.path.abspath(os.path.join(VIDEODIR, 'uploads'))

def renderTemplate(filename, config):
	tplfile = open(os.path.join(HTMLDIR, filename)).read()

	# Replace each placeholder with the information in config
	for key, value in config.items():
		tplfile = re.sub(re.escape('<!--###'+key.upper()+'###-->'), str(value), tplfile)

	return tplfile

def formatTime(frame, fps):
	lengthInSec = int(frame/fps)
	seconds = lengthInSec % 60
	minutes = int(lengthInSec / 60) % 60
	hours = int(lengthInSec / 60 / 60) % 60

	return '%1.2d' % hours + ':' + '%1.2d' % minutes + ':' + '%1.2d' % seconds

def getUploads():

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
		progress = "100%"

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

class Root(object):
	@cherrypy.expose
	def index(self):
		
		content = """<div style="padding-left:5px"><b>Latest search terms:</b><br>...<br><br><b>Latest scene searches:</b><br>...</div>"""

		uploads = getUploads()

		config = {
			'title': 'Main',
			'videocount': uploads['videocount'],
			'scenecount': uploads['scenecount'],
			'searchterm': '',
			'uploads': uploads['uploads'],
			'content': content
		}
		return renderTemplate('template.html', config)

	@cherrypy.expose
	def search(self, name = None):
		if not name:
			raise cherrypy.HTTPRedirect('/')

		videosFromDb = VIDEOS.find({"filename": { '$regex': name} })

		if videosFromDb.count() == 0:
			content = 'No Videos found, for your search query: "'+name+'".'
		else:
			videos = []

			for video in videosFromDb:
				fps = int(video['fps'])
				filename = str(video['filename'])
				vidid = str(video['_id'])

				videoconfig = {
					'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(filename))[0], 'scene0.jpeg'),
					'videoid': vidid,
					'filename': filename,
					'length': formatTime(int(video['framecount']), fps)
				}
				
				videos.append(renderTemplate('video.html', videoconfig))

			content = ""
			for video in videos:
				content += video

		uploads = getUploads()

		config = {
			'title': 'Search',
			'videocount': uploads['videocount'],
			'scenecount': uploads['scenecount'],
			'searchterm': name,
			'uploads': uploads['uploads'],
			'content': content
		}

		return renderTemplate('template.html', config)

	@cherrypy.expose
	def searchScene(self, vidid = None, frame = None):
		if not vidid or not frame:
			raise cherrypy.HTTPRedirect('/')
		
		videoFromDb = VIDEOS.find_one({'_id': str(vidid)})

		if not videoFromDb:
			content = 'No Videos found, for your search query.'
		else:
			scenes = []
			filename = videoFromDb['filename']
			fullpath = os.path.join('/videos/', filename)
			fps = videoFromDb['fps']

			for scene in videoFromDb['scenes']:

				sceneconfig = {
					'url': fullpath,
					'extension': os.path.splitext(filename)[1][1:],
					'time': str(scene['startframe'] / fps),
					'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(filename))[0], 'scene'+scene['_id']+'.jpeg'),
					'filename': filename,
					'scenecount': str(int(scene['_id'])),
					'starttime': formatTime(int(scene['startframe']), fps),
					'endtime': formatTime(int(scene['endframe']), fps)
				}

				scenes.append(renderTemplate('similarscene.html', sceneconfig))

			content = ""
			for scene in scenes:
				content += scene

		uploads = getUploads()

		config = {
			'title': 'Found Scenes',
			'videocount': uploads['videocount'],
			'scenecount': uploads['scenecount'],
			'searchterm': '',
			'uploads': uploads['uploads'],
			'content': content
		}

		return renderTemplate('template.html', config)

	@cherrypy.expose
	def video(self, vidid = None):
		if not vidid:
			raise cherrypy.HTTPRedirect('/')

		videoFromDb = VIDEOS.find_one({'_id': str(vidid)})

		if not videoFromDb:
			raise cherrypy.HTTPRedirect('/')

		scenes = []
		filename = videoFromDb['filename']
		fullpath = os.path.join('/videos/', filename)
		fps = videoFromDb['fps']

		for scene in videoFromDb['scenes']:

			sceneconfig = {
				'url': fullpath,
				'extension': os.path.splitext(filename)[1][1:],
				'time': str(scene['startframe'] / fps),
				'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(filename))[0], 'scene'+scene['_id']+'.jpeg'),
				'filename': filename,
				'scenecount': str(int(scene['_id'])),
				'starttime': formatTime(int(scene['startframe']), fps),
				'endtime': formatTime(int(scene['endframe']), fps)
			}

			scenes.append(renderTemplate('scene.html', sceneconfig))

		content = '<div class="scene-wrap">'
		for scene in scenes:
			content += scene

		content += "</div>"

		videoFromDb = VIDEOS.find_one({'_id': str(vidid)})

		fps = int(videoFromDb['fps'])
		filename = str(videoFromDb['filename'])
		vidid = str(videoFromDb['_id'])

		videoconfig = {
			'thumbnail': os.path.join('/thumbnails/', os.path.splitext(os.path.basename(filename))[0], 'scene0.jpeg'),
			'videoid': vidid,
			'filename': filename,
			'length': formatTime(int(videoFromDb['framecount']), fps)
		}
		
		content += renderTemplate('originvideo.html', videoconfig)

		uploads = getUploads()

		config = {
			'title': 'Scenes',
			'videocount': uploads['videocount'],
			'scenecount': uploads['scenecount'],
			'searchterm': '',
			'uploads': uploads['uploads'],
			'content': content
		}

		return renderTemplate('template.html', config)

	@cherrypy.expose
	def upload(self):
		filename = os.path.basename(cherrypy.request.headers['x-filename'])
		destination = os.path.join(UPLOADDIR, filename)
		with open(destination, 'wb') as f:
			shutil.copyfileobj(cherrypy.request.body, f)

		if not cutter.save_cuts(os.path.join('uploads/', filename), True)
			# TODO: Aussagekräftige Fehlermeldungen etc.
			print "Error: File already exists"

if __name__ == '__main__':
	cherrypy.config.update('./global.conf')

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

	cherrypy.server.max_request_body_size = 0

	if hasattr(cherrypy.engine, 'block'):
		# 3.1 syntax
		cherrypy.engine.start()
		cherrypy.engine.block()
	else:
		# 3.0 syntax
		cherrypy.server.quickstart()
		cherrypy.engine.start()
