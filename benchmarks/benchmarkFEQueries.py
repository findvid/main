"""
This Script will take the given paths, to get targetlists from the specified webserver.
The originalpath is the path, where the videos originally are. 
All paths of query and target videos are replaced with this path, to fulfill the requirements to the output txt.

The Script assumes, that the query videos start with 'query', target videos don't matter.
If the target video don't exist in the original path, the absolute path is written into the output txt. TODO: (CHECK IF THIS IS OKAY)
"""

import pymongo
import shutil
import os
import argparse
import re

from time import time

# instanciate and configure an argument parser
PARSER = argparse.ArgumentParser(description='')
PARSER.add_argument('database', metavar='DB',
	help='The name of the MongoDB Database on localhost')
PARSER.add_argument('collection', metavar='COLLECTION',
	help='The name of the Collection in the Database')
PARSER.add_argument('origpath', metavar='ORIGINALPATH',
	help='The original path, where the queries and targets are (relative to /video/videosearch/). This is used for the output file.')
PARSER.add_argument('realpath', metavar='REALPATH',
	help='The path, where the queries and targets are (relative to configured path in database).')

# parse input arguments
ARGS = PARSER.parse_args()

DBNAME = ARGS.database
COLNAME = ARGS.collection
ORIGPATH = ARGS.origpath
REALPATH = ARGS.realpath

# Establish MongoDb Connection and get db and video collection
MONGOCLIENT = pymongo.MongoClient(port=8099)
DB = MONGOCLIENT[DBNAME]
VIDEOS = DB[COLNAME]

# Directory for Videos (configured in CONFIG)
VIDEODIR = os.path.abspath(os.path.join(CONFIG['abspath'], CONFIG['videopath']))

if __name__ == '__main__':
	# Search for all videos starting with "query" in database
	# RegEx: Search for substring "query" in path, with digits after the string and a period after the digits
	# (so we can be sure 'query' is not some directory name or else...)
	queryvideos = VIDEOS.find({'filename': {'$regex': 'query\d*\.'}}, {'filename': 1})

	count = queryvideos.count()

	print count + " videos starting with 'query' where found in database.\n"
	userinput = input("Do you want to continue with the found videos? (Y/N)")
	
	if userinput == "N":
		print "Please enter a filename for "
	else:
		print "Continue with found videos...\n"


	return False