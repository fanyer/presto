#!/bin/bash
#
# Fetches the IANA character set list and runs the charsets.py script
# to re-generate the alias and MIB lists in this module.

[ "$PYTHON" ] || PYTHON=`which python2 2>/dev/null`
[ "$PYTHON" ] || PYTHON=python

# Quit on error
set -xe

# Retrieve list
test -e data/character-sets && rm -f data/character-sets
wget -O data/character-sets http://www.iana.org/assignments/character-sets
# There is a mirror at http://iana.netnod.se/assignments/character-sets
# in case the above fails

# Convert to CRLF to LF termination
dos2unix < data/character-sets > data/character-sets.txt && rm -f data/character-sets

# Back up generated files
test -e ../utility/aliases.inl && mv -f ../utility/aliases.inl ../utility/aliases.inl.old
test -e ../utility/mib.inl && mv -f ../utility/mib.inl ../utility/mib.inl.old

# Run the Python script
exec "$PYTHON" charsets.py
