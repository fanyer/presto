# -*- Mode: python; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-

import sys
import os
import operator

#
# global variables
#

debug = False
verbosity = 0
in_file_name = ""
hash_file_name = ""
test_factor = 33
hash_size = 4093

#
# definition of functions and classes used
#

def verbose(s, level = 0):
    global verbosity
    if verbosity >= level:
        print s

def makeHashValue(s, base, size, factor):
	val = base
#	verbose("Initial: %d" % val);
	for i in range(len(s)):
		val = (val * factor) % 4294967296
		val += ord(s[i])
#		verbose("step: %d" % val);
	return val % size

def countCollisions(attr_list, base, size, factor, hash_map):
	hash_map.clear()
	collisions = 0
	for [attr_name, ns] in attr_list:
		hash_value = makeHashValue(attr_name.lower(), base, size, factor);
		if not hash_value in hash_map:
			hash_map[hash_value] = [attr_name]
		else:
			entry = hash_map[hash_value]
			if attr_name not in entry:
				collisions += 1
				entry.append(attr_name)
				if len(entry) > 2:
					return -1
	return collisions

is_tags = True
if len(sys.argv) > 1:
	for arg in sys.argv:
		if arg == "attrs":
			is_tags = False
		elif arg == "--debug":
			debug = True
else:
	verbose("\nUsage: mkhash.py [tags | attrs]\n")
	sys.exit()

if is_tags:
	in_file_name = "html5tags.txt"
	hash_file_name = "elementhashbase.h"
	test_factor = 33
	hash_size = 2039
else:
	in_file_name = "html5attrs.txt"
	hash_file_name = "attrhashbase.h"
	test_factor = 33
	hash_size = 2039

in_file_dir = os.path.dirname(os.path.abspath(in_file_name)) + "/"
in_file = open(in_file_name, "r")

first = True
attr_map = {}
attr_list = []
attr_indices = []
attr_index = 0

for line in in_file:
	attr_entry = line.strip()
	[attr_name, ns] = attr_entry.split(',')
	if not attr_name in attr_map:
		attr_map[attr_name] = ns
	attr_list.append([attr_name, ns])

in_file.close()
attr_list.sort()
verbose("\n%d strings found in %s" % (len(attr_list), in_file_name))

# find optimal hash values

verbose("\nCalculating best hashing (may take some time)...")

best_col = 5000
best_map = {}
best_base = 5000
best_size = 3000
hash_map = {}

test_size = hash_size
#for test_size in range(400, 2039):
for test_base in range(0, 10000):
	col = countCollisions(attr_list, test_base, test_size, test_factor, hash_map)
	if col >= 0:
		if col == 0 or col < best_col:
			accepted = True
			for test_key in hash_map.keys():
				if len(hash_map[test_key]) > 2:
					accepted = False
					break

			if accepted:
				best_col = col
				best_base = test_base
				best_size = test_size
				best_map.clear()
				best_map = hash_map.copy()
		if col == 0:
			break

# write the outline of the hash table to map.html
if debug:
	best_keys = best_map.keys()
	best_keys.sort()
	out_file = open("map.html", "w")
	out_file.write("<!DOCTYPE html>\n<html>\n<head>\n<style>td { width 20em }\n.collision { background-color: red }</style>\n</head>\n<body>\n<table>\n")

	last_entry = 0
	for key in best_keys:
		for i in range(last_entry, key):
			out_file.write("<tr><td>%d</td> <td>&nbsp;</td>\n" % i)
		last_entry = key + 1
		if len(best_map[key]) > 1:
			out_file.write("<tr class=\"collision\"><td>%s</td> <td>%s</td>\n" % (key, best_map[key]))
		else:
			out_file.write("<tr><td>%s</td> <td>%s</td>\n" % (key, best_map[key]))

	out_file.write("</table>\n</body>\n</html>")
	out_file.close()

if best_col == 5000:
	verbose("\nFATAL: No good hashing found. Nothing written.\n")
else:
	verbose("\nBest hashing found.")
	verbose("\nCollisions: %d" % best_col)
	verbose("Hash array size: %d, best base: %d" % (hash_size, best_base))
	if not debug:
		hash_file = open(hash_file_name, "w")
		if is_tags:
			hash_file.write("#define HTML5_TAG_HASH_BASE %d\n#define HTML5_TAG_HASH_SIZE %d\n" % (best_base, hash_size))
		else:
			hash_file.write("#define HTML5_ATTR_HASH_BASE %d\n#define HTML5_ATTR_HASH_SIZE %d\n" % (best_base, hash_size))
		hash_file.close()
		verbose("Wrote %s\n" % hash_file_name)
