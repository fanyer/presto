# -*- Mode: python; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-

import sys
import os

#
# global variables
#

verbosity = 0
table_files = []

in_file_name = "errors.txt"
length_file_name = "errortablelength.inl"
code_file_name = "errorcodes.h"
desc_file_name = "errordescs.h"

#
# definition of functions and classes used
#

def verbose(s, level = 0):
    global verbosity
    if verbosity >= level:
        print s

#
# main program
#

file_header = """/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 *                    This file is auto generated
 *
 *                    !!! DO NOT EDIT BY HAND !!!
 *
 * Edit errors.txt and run mkerrors.py, both in modules/logdoc/scripts.
 */

"""

if not os.path.isdir("modules"):
    os.chdir(os.path.dirname(sys.argv[0]))
    while not os.path.isdir("modules"):
        prevdir = os.getcwd()
        os.chdir("..")
        if prevdir == os.getcwd():
			print "FAIL! Need to run the script from within the source tree."
            sys.exit(1)

in_path = os.path.join(os.getcwd(), "modules", "logdoc", "scripts")
out_path = os.path.join(os.getcwd(), "modules", "logdoc", "src", "html5")

in_file_path = os.path.join(in_path, in_file_name)
length_file_path = os.path.join(out_path, length_file_name)
code_file_path = os.path.join(out_path, code_file_name)
desc_file_path = os.path.join(out_path, desc_file_name)

error_list = []
in_file = open(in_file_path, "r")

for line in in_file:
    fields = line.split(':')
    if len(fields) > 1:
		error_list.append((fields[0].strip(), fields[1].strip()))

in_file.close()

length_file = open(length_file_path, "w")
length_file.write(file_header)
length_file.write("static const unsigned\tkErrorTableLength = %d;\n" % (len(error_list) + 1))
length_file.close()

code_file = open(code_file_path, "w")
desc_file = open(desc_file_path, "w")

code_file.write(file_header)
code_file.write("enum ErrorCode\n{\n\tGENERIC\n")

desc_file.write(file_header)
desc_file.write("CONST_ARRAY(g_html5_error_descs, char*)\n\tCONST_ENTRY(\"Parse error!\")\n")

for entry in error_list:
	code_file.write("\t, %s\n" % entry[0])
	desc_file.write("\t, CONST_ENTRY(\"%s\")\n" % entry[1])

code_file.write("};\n")
desc_file.write("CONST_END(g_html5_error_descs)\n")

code_file.close()
desc_file.close()

verbose("  Wrote %s" % length_file_path)
verbose("  Wrote %s" % code_file_path)
verbose("  Wrote %s" % desc_file_path)
