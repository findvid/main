import cherrypy
import pymongo
import os
import re

MONGOCLIENT = pymongo.MongoClient()
DB = MONGOCLIENT['findvid']
VIDEOS = DB['videos']

HTMLDIR = os.path.join(os.path.abspath('.'), "html")
DEBUG = True

def renderTemplate(filename, config):
	tplfile = open(os.path.join(HTMLDIR, filename)).read()

	# Replace each placeholder with the information in config
	for key, value in config.items():
		tplfile = re.sub(re.escape("<!--###"+key.upper()+"###-->"), str(value), tplfile)

	return tplfile

class Root(object):
	@cherrypy.expose
	def index(self):
		videosFromDb = VIDEOS.find()

		videos = []

		for video in videosFromDb:
			fps = int(video['fps'])
			filename = str(video['filename'])

			lengthInSec = int(int(video['framecount'])/fps)
			lengthSec = lengthInSec % 60
			lengthMin = int(lengthInSec / 60) % 60
			lengthHr = int(lengthInSec / 60 / 60) % 60

			videoconfig = {
				'thumbnail': 'images/thumb.png',
				'filename': filename,
				'length': "%1.2d" % lengthHr + ":" + "%1.2d" % lengthMin + ":" + "%1.2d" % lengthSec
			}

			scenes = []

			for scene in video['scenes']:

				startInSec = int(int(scene['startframe'])/fps)
				startSec = startInSec % 60
				startMin = int(startInSec / 60) % 60
				startHr = int(startInSec / 60 / 60) % 60
 
				endInSec = int(int(scene['endframe'])/fps)
				endSec = endInSec % 60
				endMin = int(endInSec / 60) % 60
				endHr = int(endInSec / 60 / 60) % 60

				sceneconfig = {
					'thumbnail': 'images/thumb.png',
					'filename': filename,
					'scenecount': str(int(scene['_id'])),
					'starttime': "%1.2d" % startHr + ":" + "%1.2d" % startMin + ":" + "%1.2d" % startSec,
					'endtime': "%1.2d" % endHr + ":" + "%1.2d" % endMin + ":" + "%1.2d" % endSec
				}

				scenes.append(renderTemplate('scene.html', sceneconfig))

			videos.append({'html': renderTemplate('video.html', videoconfig), 'scenes': scenes})

		content = ""
		for video in videos:
			for scene in video['scenes']:
				content += scene

		config = {
			'title': 'find.vid - The Videosearch Engine',
			'videocount': '0',
			'scenecount': '0',
			'content': content
		}
		return renderTemplate('template.html', config)

class Test(object):
	@cherrypy.expose
	def index(self):
		return "test"

if __name__ == '__main__':
	cherrypy.config.update('./global.conf')
	
	cherrypy.tree.mount(Root(), "/", './app.conf')
	cherrypy.tree.mount(Test(), "/test", './app.conf')

	if hasattr(cherrypy.engine, 'block'):
		# 3.1 syntax
		cherrypy.engine.start()
		cherrypy.engine.block()
	else:
		# 3.0 syntax
		cherrypy.server.quickstart()
		cherrypy.engine.start()
