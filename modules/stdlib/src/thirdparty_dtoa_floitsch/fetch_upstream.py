#!/usr/bin/env python
#
# Low-grade script for fetching double-conversion
# from upstream and apply local patches.
#
# To use,
#
#  foo$ cd modules/stdlib/src/thirdparty_dtoa_floitsch/
#  foo$ python fetch_upstream.py
#
# Assuming the download and application of local patches went
# without incident, upstream_new/ will have the freshly minted
# result.
#
# ..if it didn't, you will need to adjust the patches to apply
# cleanly against the current version of upstream. The library is
# relatively slow moving (Nov 2011), so if that continues to hold,
# this shouldn't be a major problem..
#
# To ease generation of new patches, supply --no-patches as option
# to this script, followed by a 'diff -ur' of the existing and
# and new src/ directories.
#
import sys, os, re, urllib2

upstream_base = "http://double-conversion.googlecode.com/git";
upstream_src_base = os.path.join(upstream_base, 'src');

# Where the thirdparty files will end up.
thirdparty_base = "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/";

# Temporary directory to use when preparing
local_upstream_dir = 'upstream_new';

local_upstream_dir_src = os.path.join(local_upstream_dir, 'src');

# Should this sprout more options, add usage of your
# favourite command-line library right here.
apply_patches = not (len(sys.argv) > 1 and sys.argv[1] == "--no-patches")

# Enumerating the files won't pick up new files..but compilation
# no doubt will. You'll need to add the new files here + extend
# stdlib_dtoa.cpp with a direct #include of the .cc files.
upstream_files = [
    'bignum.cc',
    'bignum.h',
    'bignum-dtoa.cc',
    'bignum-dtoa.h',
    'cached-powers.cc',
    'cached-powers.h',
    'diy-fp.cc',
    'diy-fp.h',
    'double-conversion.cc',
    'double-conversion.h',
    'ieee.h',
    'fast-dtoa.cc',
    'fast-dtoa.h',
    'fixed-dtoa.cc',
    'fixed-dtoa.h',
    'strtod.cc',
    'strtod.h',
    'utils.h'
]

upstream_project_files = [
    'README',
    'AUTHORS',
    'COPYING',
    'LICENSE'
]

def transform_line(line):
    line = re.sub(r'^(namespace double_conversion {)$', r'#ifndef NO_DOUBLE_CONVERSION_NAMESPACE\n\1\n#endif // !NO_DOUBLE_CONVERSION_NAMESPACE', line)
    line = re.sub(r'^(}  // namespace double_conversion)$', r'#ifndef NO_DOUBLE_CONVERSION_NAMESPACE\n\1\n#endif // !NO_DOUBLE_CONVERSION_NAMESPACE', line)
    line = re.sub(r'strlen\(', r'double_conversion_strlen(', line)
    line = re.sub(r'strcpy\(', r'double_conversion_strcpy(', line)
    line = re.sub(r'memcpy\(', r'double_conversion_memcpy(', line)
    line = re.sub(r'ceil\(', r'double_conversion_ceil(', line)
    line = re.sub(r'^#include "(.+)$', r'#include "modules/stdlib/src/thirdparty_dtoa_floitsch/upstream/src/\1', line)
    return line

if not os.path.isdir(os.path.join(sys.path[0], local_upstream_dir)):
    os.mkdir(os.path.join(sys.path[0], local_upstream_dir))
if not os.path.isdir(os.path.join(sys.path[0], local_upstream_dir_src)):
    os.mkdir(os.path.join(sys.path[0], local_upstream_dir_src))

for item in upstream_files:
    local_filename = os.path.join(sys.path[0], local_upstream_dir_src, item)
    upstream_file = os.path.join(upstream_src_base, item)
    print upstream_file + '  ==>  ' + local_filename

    with open(local_filename, 'wb') as local_file:
        for line in urllib2.urlopen(upstream_file):
            local_file.write(transform_line(line))

    if apply_patches:
        if os.path.isfile(os.path.join(sys.path[0], 'patches', item + '.patch')):
           os.system('cd ' + local_upstream_dir_src + '; patch --binary < ' + os.path.join(sys.path[0], 'patches', item + '.patch'));

for item in upstream_project_files:
    local_filename = os.path.join(sys.path[0], local_upstream_dir, item)
    upstream_url = upstream_base + '/' + item
    print upstream_url + '  ==>  ' + local_filename

    with open(local_filename, 'wb') as local_file:
        for line in urllib2.urlopen(upstream_url):
            local_file.write(line)
