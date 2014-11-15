import cherrypy
import pymongo
import shutil
import os
import re

MONGOCLIENT = pymongo.MongoClient()
DB = MONGOCLIENT['findvid']
VIDEOS = DB['videos']

HTMLDIR = os.path.join(os.path.abspath('.'), 'html')
DEBUG = True

UPLOADDIR = os.path.join('./', 'uploads')

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

	# todo: only find videos where upload=true
	uploadsFromDb = VIDEOS.find()

	uploads = []

	for upload in uploadsFromDb:
		progress = "100%"

		fps = int(upload['fps'])
		filename = str(upload['filename'])
		scenecount = len(upload['scenes'])
		vidid = str(upload['_id'])

		uploadconfig = {
			'progress': progress,
			'thumbnail': 'images/thumb.png',
			'videoid': vidid,
			'scenecount': scenecount,
			'filename': filename,
			'length': formatTime(int(upload['framecount']), fps)
		}
		
		uploads.append(renderTemplate('upload.html', uploadconfig))

	uploadsContent = ""
	for upload in uploads:
		uploadsContent += upload

	return uploadsContent

#def upload():
	#filename = os.path.basename(uploadfile.filename)
	#extension = os.path.splitext(filename)[1][1:]
#
#	#savedFile = os.path.join(UPLOADDIR, filename)
#
#	#if os.path.exists(savedFile):
#	#	return False
#
#	#fileOut = open(savedFile, "w")
#	#fileIn = uploadfile.file.read()
#	#fileOut.write(fileIn)
#	#fileOut.close()
#
	# call the python server

	#return True


class Root(object):
	@cherrypy.expose
	def index(self):
		videosFromDb = VIDEOS.find()

		videos = []

		for video in videosFromDb:
			fps = int(video['fps'])
			filename = str(video['filename'])
			vidid = str(video['_id'])

			videoconfig = {
				'thumbnail': 'images/thumb.png',
				'videoid': vidid,
				'filename': filename,
				'length': formatTime(int(video['framecount']), fps)
			}
			
			videos.append(renderTemplate('video.html', videoconfig))

		content = ""
		for video in videos:
			content += video

		config = {
			'title': 'find.vid - The Videosearch Engine: Videos',
			'videocount': '0',
			'scenecount': '0',
			'searchterm': '',
			'uploads': getUploads(),
			'content': content
		}
		return renderTemplate('template.html', config)

	@cherrypy.expose
	def upload(self):
		filename = os.path.basename(cherrypy.request.headers['x-filename'])
		destination = os.path.join(UPLOADDIR, filename)
		with open(destination, 'wb') as f:
			shutil.copyfileobj(cherrypy.request.body, f)

class Video(object):
	@cherrypy.expose
	def index(self, vidid = None):
		if not vidid:
			raise cherrypy.HTTPRedirect('/')

		videoFromDb = VIDEOS.find_one({'_id': str(vidid)})

		if not videoFromDb:
			raise cherrypy.HTTPRedirect('/')

		scenes = []
		path = videoFromDb['path']
		filename = videoFromDb['filename']
		fullpath = os.path.join(path, filename)
		fps = videoFromDb['fps']

		for scene in videoFromDb['scenes']:

			sceneconfig = {
				'url': fullpath,
				'extension': os.path.splitext(filename)[1][1:],
				'time': str(scene['startframe'] / fps),
				'thumbnail': 'images/thumb.png',
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
			'thumbnail': 'images/thumb.png',
			'videoid': vidid,
			'filename': filename,
			'length': formatTime(int(videoFromDb['framecount']), fps)
		}
		
		content += renderTemplate('originvideo.html', videoconfig)

		config = {
			'title': 'find.vid - The Videosearch Engine: Scenes',
			'videocount': '0',
			'scenecount': '0',
			'searchterm': '',
			'uploads': getUploads(),
			'content': content
		}

		return renderTemplate('template.html', config)


class Search(object):
	@cherrypy.expose
	def index(self, name = None):
		if not name:
			raise cherrypy.HTTPRedirect('/')

		videosFromDb = VIDEOS.find({"filename": { '$regex': name} })

		if not videosFromDb:
			content = 'No Videos found.'
		else:
			videos = []

			for video in videosFromDb:
				fps = int(video['fps'])
				filename = str(video['filename'])
				vidid = str(video['_id'])

				videoconfig = {
					'thumbnail': 'images/thumb.png',
					'videoid': vidid,
					'filename': filename,
					'length': formatTime(int(video['framecount']), fps)
				}
				
				videos.append(renderTemplate('video.html', videoconfig))

			content = ""
			for video in videos:
				content += video

		config = {
			'title': 'find.vid - The Videosearch Engine: Search',
			'videocount': '0',
			'scenecount': '0',
			'searchterm': name,
			'uploads': getUploads(),
			'content': content
		}

		return renderTemplate('template.html', config)


if __name__ == '__main__':
	cherrypy.config.update('./global.conf')
	
	cherrypy.tree.mount(Root(), '/', './app.conf')
	cherrypy.tree.mount(Video(), '/video', './app.conf')
	cherrypy.tree.mount(Search(), '/search', './app.conf')

	cherrypy.server.max_request_body_size = 0

	if hasattr(cherrypy.engine, 'block'):
		# 3.1 syntax
		cherrypy.engine.start()
		cherrypy.engine.block()
	else:
		# 3.0 syntax
		cherrypy.server.quickstart()
		cherrypy.engine.start()
