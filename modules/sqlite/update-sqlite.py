#!/usr/bin/python
# -*- Mode: python; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-

# This script downloads sqlite from the official website and places it in
# the sqlite module folder, ready for compilation

import optparse
import os
import pprint
import re
import shutil
import string
import sys
import tempfile
import urllib
import urllib2
import urlparse
import zipfile

download_page = "http://sqlite.org/download.html"
download_link_re = '<a[^>]+href=["\'](sqlite-amalgamation-[^"\']+)'

class URLRetreiver(urllib.FancyURLopener):
	def http_error_default(self, url, fp, errcode, errmsg, headers):
		if 400 <= errcode:
			raise urllib2.URLError(str(errcode) + " " + errmsg)

def request(url, filename = None):
	print "Requesting {0}".format(url)
	try:
		if filename is None:
			response = urllib2.urlopen(url, None, 10)
			return response.read()
		else:
			URLRetreiver().retrieve(url, filename)
	except Exception as e:
		sys.exit("Error accessing " + url + ": " + str(e))

def get_amalgamation_link():
	page_contents = request(download_page)
	download_link = re.search(download_link_re, page_contents);
	try:
		if download_link is None:
			raise IndexError()
		link = download_link.group(1)
		if link is None:
			raise IndexError()
		## resolve url: base + url
		return urlparse.urljoin(download_page, link)
	except IndexError:
		print download_link
		sys.exit("Can't find sqlite-amalgamation-xxxxxxxx.zip link")

def rm(*paths):
	for path in paths:
		try:
			if os.path.isdir(path):
				shutil.rmtree(path, True)
			else:
				os.remove(path)
		except Exception:
			pass

def read_file(path, fail_gracefully=False, firstchar=None, regexp=None):
	try:
		contents = ""
		fh = open(path, "rU")
		if regexp is None:
			contents = fh.read().strip()
		else:
			cre = re.compile(regexp)
			for line in fh.readlines():
				if firstchar in line:
					if cre.search(line):
						contents += line
		fh.close()
		return contents
	except Exception as e:
		if fail_gracefully:
			pass
		else:
			sys.exit("Error handling file " + path + ": " + str(e))

def write_file(path, contents):
	fh = open(path, "w")
	fh.write(contents)
	fh.close()

def chomp_file(from_file, to_file):
	f = open(from_file, 'r')
	t = open(to_file, 'w')
	for line in f:
		t.write(line.rstrip() + "\n")
	f.close()
	t.close()

def to_int(s):
	try:
		return int(s)
	except Exception:
		return 0

def is_version_higher(old, new):
	old = map(to_int, old.split("."))
	new = map(to_int, new.split("."))

	old_len = len(old)
	new_len = len(new)

	for i in range(0, min(old_len, new_len)):
		if old[i] < new[i]:
			return True
		elif new[i] < old[i]:
			return False

	if old_len < new_len:
		return True

	return False

## Main code path

cmd_args = optparse.OptionParser(usage = "Usage: %prog [options]")
cmd_args.add_option("-f", action="store_true", dest="force_update", help="Force update, skip version check.")
cmd_args.add_option("-c", action="store_true", dest="check_version", help="Simply check online version, don't update.")
(cmd_options, cmd_args) = cmd_args.parse_args()
if cmd_options.force_update and cmd_options.check_version:
	sys.exit("Options -f and -c are mutually exclusive")

sqlite_module_root = os.path.normpath(sys.path[0])
sqlite_version_file = os.path.join(sqlite_module_root, 'src', 'VERSION')

if not sqlite_module_root.endswith(os.path.join('modules', 'sqlite')):
	sys.exit("Please place this script inside the sqlite folder")

link = get_amalgamation_link()
filename = link.split('#')[0].split('?')[0].split('/')[-1]
if not filename.endswith('.zip'):
	sys.exit("Filename does not end with zip, do not know type of archive: " + filename)


localfilename = os.path.join(tempfile.gettempdir(), filename)
localextractfld = localfilename + '-extract'

rm(localfilename, localextractfld)

request(link, localfilename)

if not zipfile.is_zipfile(localfilename):
	sys.exit("Not a zip file: " + filename)

current_sqlite_version = read_file(sqlite_version_file, True)
if current_sqlite_version:
	print "Current SQLite version: '"+current_sqlite_version+"'"
else:
	sys.stderr.write("Warning: Could not read current SQLite version\n")
	current_sqlite_version = ""

localarchive = zipfile.ZipFile(localfilename, 'r')
new_sqlite_version = ""
# get sqlite3.h first to read the version
for file in localarchive.infolist():
	if os.path.basename(file.filename) == 'sqlite3.h':
		#print 'Extracting {0} with size {1}'.format(file.filename, file.file_size)
		localarchive.extract(file.filename, localextractfld)
		headerpath = os.path.join(localextractfld, file.filename)
		new_sqlite_version = read_file(headerpath, False, '#', '#define\s+SQLITE_VERSION\s+"').split('"')[1]
		if new_sqlite_version:
			if cmd_options.check_version:
				print "Upstream SQLite version: '"+new_sqlite_version+"'"
				rm(localfilename, localextractfld)
				sys.exit(0)
			elif is_version_higher(current_sqlite_version, new_sqlite_version):
				print "New SQLite version: '"+new_sqlite_version+"'. To revert do 'git checkout modules/sqlite/src'"
			elif cmd_options.force_update:
				print "Upstream SQLite version is the same as the local, update forced"
			else:
				print "Upstream SQLite version is the same as the local, quiting"
				rm(localfilename, localextractfld)
				sys.exit(0)
		else:
			rm(localfilename, localextractfld)
			sys.exit("Error: Could not read new SQLite version")
		chomp_file(headerpath, os.path.join(sqlite_module_root, 'src', os.path.basename(file.filename)))
		break

if not new_sqlite_version:
	rm(localfilename, localextractfld)
	sys.exit("Error: did not find sqlite3.h in archive, could not read new SQLite version")

for file in localarchive.infolist():
	if file.file_size != 0 and os.path.basename(file.filename) != 'shell.c' and os.path.basename(file.filename) != 'sqlite3.h':
		#print 'Extracting {0} with size {1}'.format(file.filename, file.file_size)
		localarchive.extract(file.filename, localextractfld)
		chomp_file(os.path.join(localextractfld, file.filename), os.path.join(sqlite_module_root, 'src', os.path.basename(file.filename)))

rm(localfilename, localextractfld)
write_file(sqlite_version_file, new_sqlite_version + "\n")

print "Update complete. Don't forget to apply the outstanding patch. See modules/sqlite/documentation/index.html"
