import sys
import os.path
import StringIO

per_path = {}

for source in sys.argv[1:]:
    path = os.path.dirname(source)
    per_path.setdefault(path, []).append(source)

for path, sources in per_path.items():
    if not os.path.isdir(path):
        os.makedirs(path)

    jumbo_cpp_path = os.path.join(path, "es_jumbo.cpp")
    jumbo_cpp = StringIO.StringIO()

    print >>jumbo_cpp, '#include "core/pch.h"'

    for source in sources:
        print >>jumbo_cpp, '#include "%s"' % source

    if not os.path.isfile(jumbo_cpp_path) or jumbo_cpp.getvalue() != open(jumbo_cpp_path).read():
        open(jumbo_cpp_path, "wct").write(jumbo_cpp.getvalue())

    print jumbo_cpp_path
