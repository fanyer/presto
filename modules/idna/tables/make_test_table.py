#! /usr/bin/python
import sys
import re

def error(message):
	print >>sys.stderr, message
	sys.exit(1)

pattern = re.compile('([\d\.A-Fa-f]+)[^;]*; ([^ ]*)')
table = []

# Open and parse the input data
try:
	file = open('test.txt', 'r')
except:
	error("Error opening tests.txt")

for line in file.readlines():
	m = pattern.match(line)
	if not m:
		continue

	# Get range and IDNAProperty value from the parsed line
	range, idnaproperty = m.group(1,2)
	idnaproperty = "SELFTEST_"+idnaproperty

	range_splited = range.split("..")
	start, end = (int(range_splited[0], 16), int(range_splited[-1], 16))

	table.append([start, idnaproperty])
	if start != end:
		table.append([int((start+end)/2), idnaproperty])
		table.append([end, idnaproperty])

# Write the requested table
# Write table used by idna selftest
for t in table:
	print "0x%.4x\t%s" % (t[0], t[1])

