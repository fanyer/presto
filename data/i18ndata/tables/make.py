#!/usr/bin/env python

import sys
from chrtblgen import main
try: ans = main(sys.argv[0], [ '-v' ] + sys.argv[1:], sys.stdout, sys.stderr)
except KeyboardInterrupt:
    sys.stderr.write('You interrupted me.  I guess you know best.\n')
    ans = 0
#except:
#    sys.stderr.write("Something went horribly wrong !\n")
#    ans = -1

sys.exit(ans)
