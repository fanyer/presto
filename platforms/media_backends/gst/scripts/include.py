#!/usr/bin/env python

import os.path
import re
import sys
import string

def main():
    scriptdir = os.path.dirname(sys.argv[0])
    gstdir = os.path.join(scriptdir, "..")
    incdir = os.path.join(gstdir, "include")

    incre = re.compile('#include\s+(<(.*)>)')

    def visit(arg, dirname, names):
        for name in names:
            fname = os.path.join(dirname, name)
            if os.path.isfile(fname) and fname.endswith(".h"):
                ftext = ""
                changed = False
                for line in file(fname):
                    m = incre.match(line)
                    if m:
                        hfile = os.path.join(incdir, m.group(2))
                        if os.path.isfile(hfile):
                            # this is a local include, rewrite it
                            line = line[:m.start(1)] + '"platforms/media_backends/gst/include/' + \
                                m.group(2) + '"' + line[m.end(1):]
                            changed = True
                    ftext += line
                if changed:
                    file(fname, 'w+').write(ftext)
                    print "%s changed" % fname

    os.path.walk(incdir, visit, None)

if __name__ == '__main__':
    main()
