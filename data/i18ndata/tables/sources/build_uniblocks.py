#!/usr/bin/env python

"""Convert ulUnicodeRange table from OpenType spec into a uniblocks.txt file.

This script will parse the documentation of the ulUnicodeRange bitfield in the
OpenType specification (http://www.microsoft.com/typography/otspec/os2.htm),
and generate a uniblocks.txt file suitable for use by the chartable generator.

Command-line options:
    --input=filename   Alternative input file (defaults to ./os2.html)
    --output=filename  Alternative output file (defaults to ./uniblocks.txt)
    --help             Print this help message
"""

# Copyright Opera software ASA, 2008.  Written by JohanH.
import sys
from StringIO import StringIO

class Entry:
	def __init__(self, bit = None, name = None, start = None, end = None):
		self.bit   = bit
		self.name  = name
		self.start = start
		self.end   = end

	def isValid(self):
		for val in vars(self).itervalues():
			if val is None: return False
		if self.start >= self.end: return False
		return True

	def isEmpty (self):
		return self.end - self.start <= 0

	def intersection(self, other):
		"""Calculate range intersection between self and other.

		Return a new entry whose range describes the intersection
		between self's range and other's range.
		Return None if no intersection
		"""
		if self.end < other.start or other.end < self.start:
			return None
		elif self.isEmpty() or other.isEmpty():
			return None
		else:
			return Entry(start = max(self.start, other.start), \
			             end   = min(self.end,   other.end))

	def remove(self, other):
		"""Remove other's range from self.

		Change self so that it no longer intersects with other's range.
		If this causes self's range to be split into 2 non-empty ranges,
		self is changed to refer to the first of these ranges, and a new
		entry is created to refer to the second range. The new entry is
		returned. If no second entry was created, None is returned.
		If other's completely overlaps self, make self's range empty
		(this will cause self.isValid() to return False).
		"""
		other = self.intersection(other)
		if other is None: return None
		part1Size = other.start - self.start
		part2Size = self.end - other.end
		assert part1Size >= 0 and part2Size >= 0
		if part1Size and part2Size: # Must into split 2 non-empty ranges
			secondRange = Entry(
				self.bit, self.name, other.end + 1, self.end)
			self.end = other.start - 1
			assert self.isValid()
			assert secondRange.isValid()
			return secondRange
		elif part1Size: # Must move self.end down to before other.start
			self.end = other.start - 1
			assert self.isValid()
			return None
		elif part2Size: # Must move self.start up to after other.end
			self.start = other.end + 1
			assert self.isValid()
			return None
		else: # Must remove entire range of self
			assert self.start == other.start \
			   and self.end == other.end
			self.end = self.start # Make self's range empty.
			assert not self.isValid()
			return None

	def __str__(self):
		return "%(bit)s;%(start)04x;%(end)04x;%(name)s" % vars(self)

def addParsedEntry(entry, parsedData):
	"""Add the given entry to the list of parsed entries"""

	# Skip invalid entries
	if not entry.isValid(): return

	# Detect overlap with existing entries
	for i in range(len(parsedData)):
		intersection = entry.intersection(parsedData[i])
		if intersection:
			# Entry overlaps with parsedData[i]
			# Assume that later entries override earlier ones.
			# Therefore remove the intersecting range from
			# parsedData[i] (potentially splitting it in twice).
			# If parsedData[i] is left completely empty, is will
			# .isValid(), which shall cause us to ignore it when
			# generating the output file
			print "Overlapping ranges: '%s' overrides '%s'" \
				% (entry, parsedData[i])
			afterSplit = parsedData[i].remove(intersection)
			if afterSplit: parsedData.append(afterSplit)

	parsedData.append(entry)

def parseTableRows(rowData, parsedData):
	States = ('NewRow', 'Cell1', 'Cell2', 'Cell3', 'EndRow')
	state = 'NewRow'
	currentEntry = None
	for line in rowData:
		line = line.strip()
		if not line: continue # Skip empty lines

		if state == 'NewRow':
			assert line == '<TR>'
			currentEntry = Entry()
			state = 'Cell1'
		elif state == 'Cell1':
			assert line.startswith('<TD CLASS=TAB VALIGN=TOP>')
			assert line.endswith('</TD>')
			contents = line[25:-5].strip()
			if contents == '&nbsp;':
				# Reuse last bit if not given
				currentEntry.bit = parsedData[-1].bit
			elif contents.isdigit():
				currentEntry.bit = int(contents)
			else:
				# NaN
				pass
			state = 'Cell2'
		elif state == 'Cell2':
			assert line.startswith('<TD CLASS=TAB VALIGN=TOP')

			# The very last entry in the table has only 2 cells
			if line.endswith('</TR>'):
				state = 'NewRow'
				continue

			# Some lines lack the '>' in the initial <TD>
			i = 25 # contents start here
			if line[24] != '>':
				i = 24

			assert line.endswith('</TD>')
			contents = line[i:-5].strip()
			currentEntry.name = contents
			state = 'Cell3'
		elif state == 'Cell3':
			# Some lines contain an improperly nested <span>. E.g:
			# <TD ...>0C00-<span ...>0C7F</TD></span></TR>
			i = line.lower().find('<span')
			if i >= 0:
				post = line[line.index('>', i + 5) + 1:]
				line = line[:i] + post.replace("</span>", "")

			# Some lines end with </TD></TR>, some with only </TD>
			if line.endswith('</TR>'):
				# Ends with </TD></TR>
				state = 'NewRow' # Next line starts a new row
				line = line[:-5] # Chop off </TR>
			else:
				state = 'EndRow' # Next line ends current row

			# Handle stray </TD>s...
			while line.endswith('</TD></TD>'):
				line = line[:-5]

			assert line.endswith('</TD>')
			contents = line[25:-5].strip()
			(start, end) = contents.split('-')
			currentEntry.start = int(start, 16)
			currentEntry.end = int(end, 16)

			# Finished parsing current entry
			addParsedEntry(currentEntry, parsedData)
			currentEntry = None
		elif state == 'EndRow':
			assert line == '</TR>'
			state = 'NewRow'
		else:
			assert state not in States
			print "This shouldn't happen"
			sys.exit(1)

def parseInFile(inFile, parsedData):
	doc = inFile.read() # Slurp in entire document

	try:
		# Find start of ulUnicodeRange section
		i = doc.index('<a name="ur">ulUnicodeRange')
		# Proceed to table header
		i = doc.index('>Block range</TD>', i)
		# Proceed to first row of table data
		i = doc.index('<TR>', i)
		# Find end of table
		j = doc.index('</TABLE>', i)
		# Table rows are now located in doc[i:j]
		# Parse table rows
		parseTableRows(StringIO(doc[i:j]), parsedData)
	except (IndexError, ValueError, AssertionError):
		print "Failed to parse OpenType spec at %s" % (inFile.name)
		sys.exit(1)

def usage():
	# Use this file's docstring as a usage string
	print __doc__
	sys.exit(1)

def main (args = []):
	# Default in/out filenames
	inFilename  = "os2.html"
	outFilename = "uniblocks.txt"

	# Command-line argument processing
	for arg in args[1:]:
		value = None
		if arg.count('='): value = arg.split('=', 1)[1].strip()
		if arg.startswith("--input="):
			inFilename = value
		elif arg.startswith("--output="):
			outFilename = value
		elif arg.startswith("--help"):
			usage()
		else:
			print "Unknown command-line argument '%s'" % (arg)
			usage()

	# List of entries parsed from file. Entries are 4-tuples of the form:
	# (Bit, Unicode Range, Block range (start), Block range (end))
	data = []

	if inFilename == '-':
		f = sys.stdin
	else:
		f = file(inFilename)
	print "Reading ulUnicodeRange documentation from %s" % (f.name)

	# Parse input file into list of data entries
	parseInFile(f, data)
	f.close()
	print "Parsed %i blocks from ulUnicodeRange documentation" % (len(data))

	if outFilename == '-':
		f = sys.stdout
	else:
		f = file(outFilename, 'w')
	print "Writing Unicode blocks to %s" % (f.name)

	# Write data entries to output file
	for i in range(len(data)):
		entry = data[i]
		# Skip invalid entries
		if not entry.isValid(): continue
		# Also check that ranges don't overlap
		for j in range(i): assert entry.intersection(data[j]) is None, \
			"Overlapping entries: '%s' and '%s'" % (entry, data[j])
		# Finally write entry to output file
		print >>f, entry
	f.close()
	print "Successfully completed"

if __name__ == '__main__':
	sys.exit(main(sys.argv))
