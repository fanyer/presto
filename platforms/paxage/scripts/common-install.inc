@@# -*- mode: sh -*-
@@ifdef opera
if [ -f @@{ETC}/opera6rc ]
then
	mv -f @@{ETC}/opera6rc @@{ETC}/operaprefs_default.ini
fi
if [ -f @@{ETC}/opera6rc.fixed ]
then
	mv -f @@{ETC}/opera6rc.fixed @@{ETC}/operaprefs_fixed.ini
fi
@@endif
