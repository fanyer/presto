@@# -*- mode: sh -*-
@@ifdef tar
 @@define VPREFIX $PREFIX
@@else
 @@define VPREFIX @@{PREFIX}
@@endif
available()
{
	command -v "$1" >/dev/null 2>&1
}

lintian_workaround_check_for_existence_of_updatemenus()
{
	# This function is never called and is just here to keep the
	# ubuntu software center from raising ugly errors.  We do
	# check for the existence of update-menus before invoking it.
	# We just do it in a way that lintian doesn't recognize.
	-x update-menus
}

finalize_desktop()
{
	# Menu entries
	if available update-desktop-database
	then
		update-desktop-database @@{VPREFIX}/share/applications
	fi
@@ifdef deb
	if available update-menus
	then
		update-menus
	fi
	if available update-app-install
	then
		update-app-install
	fi
	if available update-software-center
	then
		update-software-center
	fi
@@endif

	# MIME associations
	if available update-mime-database
	then
@@ifdef tar
		mkdir -p @@{VPREFIX}/share/mime/packages
@@endif
		update-mime-database @@{VPREFIX}/share/mime
@@ifdef tar
		! rmdir @@{VPREFIX}/share/mime/packages >/dev/null 2>&1
@@endif
	fi

	# Icons
	touch -c @@{VPREFIX}/share/icons/hicolor
	if available update-icon-caches
	then
		update-icon-caches -tq @@{VPREFIX}/share/icons/hicolor
	elif available gtk-update-icon-cache
	then
		gtk-update-icon-cache -tq @@{VPREFIX}/share/icons/hicolor
	fi
}
