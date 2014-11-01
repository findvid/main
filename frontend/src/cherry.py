import cherrypy
import os
import re

HTMLDIR = os.path.join(os.path.abspath('.'), u"html")
DEBUG = True

def renderTemplate(config):
	tplfile = open(os.path.join(HTMLDIR, u"template.html")).read()

	# Check if there is content specified in config
	try:
		config['content']
	except NameError:
		config['content'] = None

	# If there is no content in config, raise ServerError in debug or just return unrendered template
	if config['content'] is None:
		if DEBUG:
			raise cherrypy.HTTPError(500, "The Server couldn't create the page due to a misconfiguration.")
		else:
			return tplfile

	# Replace each placeholder with the information in config
	for key, value in config.items():
		tplfile = re.sub(re.escape("<!--###"+key.upper()+"###-->"), str(value), tplfile)

	return tplfile

class Root(object):
	@cherrypy.expose
	def index(self):
		config = {
			'title': 'find.vid - Testingpage',
			'videocount': '0',
			'scenecount': '0',
			'content': 'empty'
		}
		return renderTemplate(config)

if __name__ == '__main__':
	cherrypy.config.update('./global.conf')
	
	cherrypy.tree.mount(Root(), "/", './app.conf')

	if hasattr(cherrypy.engine, 'block'):
		# 3.1 syntax
		cherrypy.engine.start()
		cherrypy.engine.block()
	else:
		# 3.0 syntax
		cherrypy.server.quickstart()
		cherrypy.engine.start()