#!/bin/sh
#
# Please use diff -urN to generate new patches.
#
# Also consider using git commands instead to be nice to platforms where diff and patch are not present.

# Don't convert line endings on Windows
if [ "$WINDIR" ]; then
	PATCHARGS=--binary
fi

echo "Rebuilding npapi.h"
cp upstream-npapi.h npapi.h
patch $PATCHARGS < patches/npapi-001-symbian_qt.patch
patch $PATCHARGS < patches/npapi-002-maemo.patch
patch $PATCHARGS < patches/npapi-004-struct-pack.patch
patch $PATCHARGS < patches/npapi-005-drop-carbon-for-mac.patch
patch $PATCHARGS < patches/npapi-006-protect-x11-types.patch
patch $PATCHARGS < patches/npapi-007-ns4p-unix-platform.patch

echo "Rebuilding nptypes.h"
cp upstream-nptypes.h nptypes.h
patch $PATCHARGS < patches/nptypes-002-brew.patch

echo "Rebuilding npruntime.h"
cp upstream-npruntime.h npruntime.h
patch $PATCHARGS < patches/npruntime-001-struct-pack.patch

echo "Rebuilding npfunctions.h"
cp upstream-npfunctions.h npfunctions.h

echo "Warning: when committing changes to MPL licensed NPAPI files in git (npapi.h, nptypes.h and npfunctions.h), you must do the same updates on the publically available MPL licensed NPAPI files: http://sourcecode.opera.com/npapi"
