#!/usr/bin/env python

#
# Copyright (C) 2009 Opera Software ASA.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.
#
# Martin Olsson
#


'''
INSTRUCTIONS

 To make lingogi use a local webserver (for manual testing):
 - start this script
 - modify ./platforms/core/tweaks.h and add the defines below
 - rebuild lingogi

	#undef TWEAK_GOGI_SITEPATCH_SPOOF_URL
	#define TWEAK_GOGI_SITEPATCH_SPOOF_URL          YES
	#define GOGI_SITEPATCH_SPOOF_URL "http://localhost:8000/"

	#undef TWEAK_GOGI_SITEPATCH_DELAY
	#define TWEAK_GOGI_SITEPATCH_DELAY              YES
	#define GOGI_SITEPATCH_DELAY                    1
'''

import BaseHTTPServer
import urllib

PORT = 8000

class MyHttpHandler(BaseHTTPServer.BaseHTTPRequestHandler):
	def do_GET(self):
		self.send_response(200)
		self.send_header('Content-type', 'application/x-opera-configuration-siteprefs')
		self.end_headers()
		self.wfile.write("""<?xml version="1.0" encoding="ISO-8859-1"?>
<preferences clean_all="1">
<host name="starbuckscoffee42.com">
	<section name="User Agent">
		<pref name="Spoof UserAgent ID" value="3"/>
	</section>
</host>
</preferences>""")

def main():
	httpd = BaseHTTPServer.HTTPServer(('', PORT), MyHttpHandler)
	print "serving at port", PORT
	httpd.serve_forever()

main()
