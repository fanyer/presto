Do NOT patch npapi.h, nptypes.h, npruntime.h or npfunctions.h directly!

Instead:
1. Create separate patch files in patches/ (include ownership and cause),
2. explicitly add their application to patch.sh, and
3. run patch.sh.
