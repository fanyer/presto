#!/usr/bin/env python
"""Use javascript to do HTML markup on the BNFs in the documentation.
This depends on the executable jsshell.exe which is found in ecmascript's
standalone project es-standalone (use the core-2 tag). Files with a BNF
will have it's 'rendered' version in the subfolder fixed/."""


import os
import re
import sys
import tempfile
import platform

jsexecutable = 'jsshell' if platform.system() != 'Windows' else 'jsshell.exe'

re_grammar = re.compile('<pre\s+id\s*=\s*[\'"]grammar[\'"]\s*>([^<]*)')
re_markedup = re.compile('\[\[BEGIN\]\]((\n|.)*)\[\[END\]\]')

def getoutput(args):
	#import commands
	#return commands.getoutput(' '.join(args))

	from subprocess import Popen,PIPE
	(out, err) = Popen(args, stdout=PIPE,stderr=PIPE).communicate()
	return out.replace('\r', '')

# Write content to file and return the filename
def writeToTmpFile(content):
	(handle, tmpfilename) = tempfile.mkstemp()
	f = open(tmpfilename, 'w')
	f.write(js)
	f.close()
	os.close(handle)
	return tmpfilename

files = os.listdir('.')

try:
	os.mkdir('fixed')
except:
	pass

for file in files:
	try:
		content = open(file).read().replace('\r', '')
	except:
		continue
	match = re_grammar.search(content)
	if match:
		grammar = match.groups()[0]
		formatted_grammar = grammar.replace('\n', '\\n\\\n').replace('"', '\\"')
		js = open('markup_grammar.js').read()
		js += 'grammar="' + formatted_grammar + '"\n'
		js += 'alert("[[BEGIN]]"); alert(markup(grammar)); alert("[[END]]");'
		tmpfilename = writeToTmpFile(js)
		markedup_grammar = re_markedup.search(getoutput([jsexecutable, tmpfilename])).groups()[0]
		content = re.sub(re_grammar, '<pre>' + markedup_grammar + '</pre>', content)
	#else:
	#	print "No grammar for file '%s'." % file
	
	open(os.path.join('fixed', file), 'w').write(content.replace('"../../coredoc/coredoc.css"', '"coredoc.css"'))

