#!/usr/bin/env python
#
# Prepare a file containing data tables for the FallbackThaiUnicodeLinebreaker
# class, based on data from the Thai wrap UserJS. The generated file will be
# #included by the FallbackThaiUnicodeLinebreaker implementation.
#
# See http://userjs.org/scripts/browser/enhancements/thai-wrap for more details
# on the UserJS.

import sys
import os
from optparse import OptionParser


# Default location of Thai wrap UserJS
Default_infile = "../data/thai-wrap.js"

# Default location of generated table file
Default_outfile = "../tables/unicode_linebreak_thai_fallback.inl"


def remove_comments (line, state = None):
	"""Remove "/* ... */" and "// ... \n" comments from the given line.

	Return the non-comment portion of the line, and a state containing the
	active comment type at the end of the line. This state should be
	passed along with the next line to ensure correct operation."""
	CommentTypes = [
		# start, end,  remove contents, handle backslash escape
		( '"',   '"',  False,           True),
		( "'",   "'",  False,           True),
		( "//",  "\n", True,            False),
		( "/*",  "*/", True,            False),
	]

	pre = ""
	if state:
		# look for end of comment
		pre, sep, line = line.partition(state[1])
		# skip to next if backslash-escaped
		while state[3] and pre.endswith("\\"):
			pre += sep
			pre2, sep, line = line.partition(state[1])
			pre += pre2
		if state[2]: # remove contents
			pre = ""
		else: # keep contents
			pre += sep
		if not sep: # EOC not found: line is all comment
			assert line == ""
			return (pre, state)

		# found end of comment
		state = None

	# look for first comment starter
	found_i = -1 # index in line at which comment starts
	for j, s in enumerate(CommentTypes):
		i = line.find(s[0]) # look for start of comment
		if i == -1:
			continue
		if found_i == -1 or i < found_i:
			found_i = i
			state = s

	if found_i == -1: # not found: entire line is non-comment
		assert state is None
		return (pre + line, None)

	# found: call recursively with remainder of line
	after_sep = found_i + len(state[0])
	if state[2]: # remove comments
		pre += line[:found_i]
	else: # keep comments
		pre += line[:after_sep] # include comment starter
	line, state = remove_comments(line[after_sep:], state)
	return (pre + line, state)


def is_thai_char (c):
	"""Return True iff the given unicode character is within the Thai set"""
	return 0x0e00 <= ord(c) < 0x0e60


def word2unicode (word):
	"""Convert the given unicode-value-escaped word to a unicode word

	The given word must have each letter on the form \uXXXX (where XXXX is
	a 4-digit hexadecimal number). This is turned into a proper unicode
	string, one character for each XXXX codepoint in the given word."""
	ret = []
	vals = word.split("\\u")
	assert not vals[0]
	for val in vals[1:]:
		assert len(val) == 4
		ret.append(unichr(int(val, 16)))
	assert len(ret) == len(word) // 6 # 6 chars per codepoint
	return u"".join(ret)


def parse_subregexp_set (s):
	"""Return the characters matched by the given subregexp set"""
	i = 0
	chars = []
	while i < len(s):
		if s[i:].startswith("\u"):
			chars.append(word2unicode(s[i : i + 6]))
			i += 6
		elif s[i] == "-":
			chars.append("-")
			i += 1
		else:
			assert False, "Failed to parse '%s'" % (s[i:])

	for i, c in enumerate(chars):
		if c == "-":
			# Generate range from chars[i-1] to chars[i+1]
			low = ord(chars[i - 1])
			high = ord(chars[i + 1])
			chars[i:i] = map(unichr, range(low + 1, high))
	return chars


def parse_subregexp (s):
	"""Return the characters being matched by the given sub-regexp

	Examples of sub-regexps that are parsed by this function:
		[\u0e40-\u0e44]|\\(|\\[|\\{|\"
		\u0e2f|[\u0e30-\u0e3A]|[\u0e45-\u0e4e]|\\)|\\]|\\}|\"
	"""
	ret = [] # list of chars being matched
	for comp in s.split("|"):
		if comp.startswith("[") and comp.endswith("]"):
			ret.extend(parse_subregexp_set(comp[1:-1]))
		elif comp.startswith("\u") and len(comp) == 6:
			ret.append(word2unicode(comp))
		else:
			# Strip leading backslashes
			c = comp.lstrip("\\")
			if (len(c) == 1):
				ret.append(unicode(c))
			else:
				assert False, "Failed to parse '%s'" % (comp)
	return ret


def parse_userjs (f):
	"""Read UserJS from the given filehandle, and return pertinent data"""
	ret = {}
	comment_state = None
	for line in f:
		(line, comment_state) = remove_comments(line, comment_state)
		line = line.strip()
		if not line:
			continue

		# common, unambiguous words
		if line.startswith('var cw="('):
			assert line.endswith(')";')
			words = line[9 : -3].split("|")
			ret["Wordlist"] = map(word2unicode, words)
		# leading chars
		elif line.startswith('var lc="'):
			assert line.endswith('";')
			leading_chars = parse_subregexp(line[8 : -2])
			leading_chars = filter(is_thai_char, leading_chars)
			ret["LeadingChars"] = leading_chars
		# following chars
		elif line.startswith('var fc="'):
			assert line.endswith('";')
			following_chars = parse_subregexp(line[8 : -2])
			following_chars = filter(is_thai_char, following_chars)
			ret["FollowingChars"] = following_chars
		# end-of-word chars
		elif line.startswith('r[5]=new RegExp("('):
			assert line.endswith(')");')
			eow_chars = parse_subregexp(line[18 : -4])
			eow_chars = filter(is_thai_char, eow_chars)
			ret["EOWChars"] = eow_chars
	return ret


def write_cpp_header (f, filename):
	print >>f, """\
/** @file %s
 * This file is auto-generated by
 * modules/unicode/scripts/make_unicode_linebreak_thai_fallback_data.py.
 * DO NOT EDIT THIS FILE MANUALLY.
 */
#ifdef USE_UNICODE_INC_DATA
""" % (filename)

def write_cpp_footer (f, filename):
	print >>f, """\
#endif // USE_UNICODE_INC_DATA
"""

def cppify_name (name):
	return "FallbackThaiUnicodeLinebreaker::" + name


def write_cpp_array_len (f, name, l):
	f.write("\nconst size_t %s_len = %u;\n" % (name, len(l)))


def write_cpp_uni_char_array (f, name, l):
	f.write("\nconst uni_char %s[] = {" % (name))
	for i, c in enumerate(l):
		if not i % 8:
			if i > 0:
				f.write(",")
			f.write("\n\t")
		else:
			f.write(", ")
		f.write("0x%04x" % (ord(c)))
	f.write("\n};\n")


def flatten_word_list (l):
	# Convert given list of strings "l" into a string "s" and a list "i".
	# "s" is athe concatenation of the strings in "l", and "i" is a list of
	# indices into "s" where each string from "l" can be found.
	indices = []
	for w in l:
		i = len(w)
		if len(indices):
			i += indices[-1]
		indices.append(i)
	return ("".join(l), indices)


def write_cpp_word_list (f, name, l):
	# Generate 2 arrays and an integer constant:
	# 1. const uni_char %(name)s[]: Array of uni_chars containing all the
	#    words (in sorted order), lumped together into a single string
	#    (no NUL-terminators)
	# 2. const unsigned short %(name)s_indices[]: Array of indices into
	#    above array where each word starts.
	#    The missing first entry is implicitly 0, and the last entry is
	#    length of above array.
	# 3. const size_t %(name)s_len: Length of %(name)s_indices[] array.
	chars, indices = flatten_word_list(sorted(l))
	write_cpp_uni_char_array (f, name, chars)
	f.write("\nconst unsigned short %s_indices[] = {" % (name))
	for i, n in enumerate(indices):
		if not i % 8:
			if i > 0:
				f.write(",")
			f.write("\n\t")
		else:
			f.write(", ")
		f.write("%3u" % (n))
	f.write("\n};\n")
	write_cpp_array_len(f, name, indices)


def write_cpp_char_list (f, name, l):
	write_cpp_uni_char_array(f, name, sorted(l))
	write_cpp_array_len(f, name, l)


def main (args):
	# Parse arguments
	parser = OptionParser(usage = "%prog [options]...",
		description = "Generate FallbackThaiUnicodeLinebreaker data "
			"tables from Thai wrap UserJS")
	parser.add_option("-i", "--input", dest = "infile",
		help = "location of Thai wrap UserJS", metavar = "FILE")
	parser.add_option("-o", "--output", dest = "outfile",
		help = "where to put generated tables file", metavar = "FILE")
	parser.set_defaults(infile = Default_infile, outfile = Default_outfile)
	(options, args) = parser.parse_args(args)
	assert len(args) == 1 # There should be no more arguments

	if options.infile == "-":
		f = sys.stdin
	else:
		f = open(options.infile)
	p = parse_userjs(f)
	f.close()

	if options.outfile == "-":
		f = sys.stdout
	else:
		f = open(options.outfile, "w")
	write_cpp_header(f, os.path.basename(options.outfile))
	write_cpp_word_list(f, cppify_name("Wordlist"), p["Wordlist"])
	for char_list in ("LeadingChars", "FollowingChars", "EOWChars"):
		write_cpp_char_list(f, cppify_name(char_list), p[char_list])
	write_cpp_footer(f, os.path.basename(options.outfile))
	f.close()

	return 0


if __name__ == '__main__':
	sys.exit(main(sys.argv))
