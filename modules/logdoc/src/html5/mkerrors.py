# -*- Mode: python; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-

import sys
import os

#
# global variables
#

verbosity = 0
table_files = []
in_file_name = ""

#
# definition of functions and classes used
#

def verbose(s, level = 0):
    global verbosity
    if verbosity >= level:
        print s

if len(sys.argv) < 2:
    print "FAIL! Too few arguments\n"
    sys.exit()

in_file_name = sys.argv[1]
in_file_dir = os.path.dirname(os.path.abspath(in_file_name)) + "/"

code_file_name = in_file_dir + "errorcodes.h"
desc_file_name = in_file_dir + "errordescs.h"

in_file = open(in_file_name, "r")
code_file = open(code_file_name, "w")
desc_file = open(desc_file_name, "w")

code_file.write("enum ErrorCode\n{\n\tGENERIC\n")
desc_file.write("CONST_ARRAY(g_html5_error_descs, char*)\n\tCONST_ENTRY(\"Parse error!\")\n")

for line in in_file:
    fields = line.split(':')

    if len(fields) > 1:
        code_file.write("\t, %s\n" % fields[0].strip())
        desc_file.write("\t, CONST_ENTRY(\"%s\")\n" % fields[1].strip())

code_file.write("};\n")
desc_file.write("CONST_END(g_html5_error_descs)\n")

code_file.close()
desc_file.close()

verbose("  Wrote %s" % code_file_name);
verbose("  Wrote %s" % desc_file_name);
