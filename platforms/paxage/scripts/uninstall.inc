@@# -*- mode: sh -*-

usage()
{
	version
	cat <<EOF

Options:
    --text          Select plaintext user interface
    --unattended    Ask no questions
    --quiet         Ask no questions and don't show progress (implies --text)
    --force         Ignore all kinds of errors and try to continue
    --help          Show this message
EOF
}

uninstall_file()
{
	if $upgrade_mode
	then
		mv "$PREFIX/$3" "$PREFIX/$3~"
		echo "mv '$PREFIX/$3~' '$PREFIX/$3'" >>$install_tempdir/rollback
		echo "rm -f '$PREFIX/$3~'; rmdir -p '$PREFIX/${3%/*}' 2>/dev/null" >>$install_tempdir/commit
	else
		rm -f "$PREFIX/$3"
		rmdir -p "$PREFIX/${3%/*}" 2>/dev/null
	fi
	ui_progress_advance
}

common_init
ui_init 'Uninstalling @@{S_PRODUCT}'

upgrade_mode=false

while [ -n "$1" ]
do
	common_opt "$1" || case "$1" in
		--upgrade)
			shift
			upgrade_mode=true
			talk=false
			interactive=false
			driver=plain
			force=true
			install_tempdir="$1"
			;;
		*)
			driver=plain
			usage
			exit
			;;
	esac
	shift
done

if $talk
then
	msg="This script uninstalls @@{S_PRODUCT} @@{CONF:version} build @@{CONF:build} for @@{CONF:os} on @@{TRADITIONAL_ARCH} installed as $PREFIX/bin/opera$SUFFIX."
	if $interactive
	then
		ui_warn "$msg"
	else
		ui_message "$msg"
	fi
fi

if [ ! -w $PREFIX ] && ! $force
then
	ui_error "You do not have write permissions for $PREFIX. Perhaps you should be uninstalling as root?"
fi

ui_progress_begin "Removing files"
iterate uninstall_file
ui_progress_end

if ! $upgrade_mode
then
	if $talk
	then
		ui_info "Finalizing uninstallation..."
	fi
	finalize_desktop
fi

if $talk
then
	ui_message "@@{S_PRODUCT} has been uninstalled successfully. However, users' profile directories, which are by default ~/.opera$SUFFIX, have been left intact. You might want to delete them manually."
fi
