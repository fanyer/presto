#!/usr/bin/env python

import sys, os, re, urllib2

npapi_header_files = [
	{ 'local_filename': 'upstream-npapi.h',		'upstream_url': 'http://npapi-sdk.googlecode.com/svn/trunk/headers/npapi.h'},
	{ 'local_filename': 'upstream-npfunctions.h',	'upstream_url': 'http://npapi-sdk.googlecode.com/svn/trunk/headers/npfunctions.h'},
	{ 'local_filename': 'upstream-npruntime.h',	'upstream_url': 'http://npapi-sdk.googlecode.com/svn/trunk/headers/npruntime.h'},
	{ 'local_filename': 'upstream-nptypes.h',	'upstream_url': 'http://npapi-sdk.googlecode.com/svn/trunk/headers/nptypes.h'},
]

for item in npapi_header_files:
	full_local_filename = os.path.join(sys.path[0], item['local_filename'])
	print item['upstream_url'].ljust(55) + '  ==>  ' + full_local_filename

	with open(full_local_filename, 'w') as local_file:
		for line in urllib2.urlopen(item['upstream_url']):
			local_file.write(line)
