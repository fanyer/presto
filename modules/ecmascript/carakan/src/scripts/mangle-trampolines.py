import sys
import re

comment = None

for line in sys.stdin.readlines():
    m = re.match("^\s+(// .*)$", line)
    if m:
        comment = m.group(1)
        continue

    m = re.match("^\s*(?:0x[0-9a-f][0-9a-f],\s*)+$", line)
    if m:
        text = m.group(0).rstrip()
        sys.stdout.write(text)

        if len(text) >= 40:
            sys.stdout.write("\n                                        %s" % comment)
        else:
            sys.stdout.write((" " * (40 - len(text))) + comment)

        sys.stdout.write("\n")
        continue

    sys.stdout.write(line)

    
