#!/usr/bin/env python

"Simple tool for accessing Google's Location Provider API"
# Copyright (C) 2009 Opera Software ASA.  All rights reserved.
# Johannes Hoff <jhoff@opera.com>

import json
import httplib

# Test data is wifi towers near Opera HQ
testdata = json.dumps({
	"version":"1.1.0",
	"wifi_towers":[
		{
			"mac_address":"00-21-29-93-52-7b",
			"ssid":"wiifi",
			"signal_strength":-88
		},
		{
			"mac_address":"00-14-51-69-bf-b9",
			"ssid":"flyplassen",
			"signal_strength":-88
		},
		{
			"mac_address":"00-23-69-1a-58-5c",
			"ssid":"wiifi",
			"signal_strength":-49
		},
		{
			"mac_address":"00-1c-10-44-11-f4",
			"ssid":"AlienTest",
			"signal_strength":-36
		},
	]
})

headers = {"Content-type": "application/json"}
conn = httplib.HTTPConnection("www.google.com:80")
conn.request("POST", "/loc/json", testdata, headers)
response = conn.getresponse()

if response.status == 200:
	rawdata = response.read()
	data = json.loads(rawdata)
	print "Reply:"
	print json.dumps(data, indent=3)
	print
	print "Google maps URL:"
	print "http://maps.google.com/maps?q=%f,%f" % (data['location']['latitude'], data['location']['longitude'])
else:
	print response.status, response.reason
	print response.read()
