#!/bin/sh

if [ "$3" = "0" ] ; then
	exit;
fi

run_update=0

if [ -e platforms/windows/Opera.vcxproj ] ; then
	if ! git diff --quiet $1 $2 -- platforms/windows/Opera.vcxproj.template ; then
		echo Removing Opera.vcxproj
		rm platforms/windows/Opera.vcxproj
		run_update=1
	fi
else
	run_update=1
fi

if [ -e platforms/windows/Opera.vcxproj.filters ] ; then
	if ! git diff --quiet $1 $2 -- platforms/windows/Opera.vcxproj.filters.template ; then
		echo Removing Opera.vcxproj.filters
		rm platforms/windows/Opera.vcxproj.filters
		run_update=1
	fi
else
	run_update=1
fi

if [ -e platforms/windows/plugin_wrapper.vcxproj ] || [ -e platforms/windows/plugin_wrapper_32.vcxproj ] ; then
	`git diff --quiet $1 $2 -- platforms/windows/plugin_wrapper.vcxproj.template`
	upstream_changed=$?	# exit code

	if [ "$upstream_changed" = "1" ] && [ -e platforms/windows/plugin_wrapper.vcxproj ] ; then
		echo Removing plugin_wrapper.vcxproj
		rm platforms/windows/plugin_wrapper.vcxproj
		run_update=1
	fi
	if [ "$upstream_changed" = "1" ] && [ -e platforms/windows/plugin_wrapper_32.vcxproj ] ; then
		echo Removing plugin_wrapper_32.vcxproj
		rm platforms/windows/plugin_wrapper_32.vcxproj
		run_update=1
	fi
else
	run_update=1
fi

if [ -e platforms/windows/plugin_wrapper.vcxproj.filters ] || [ -e platforms/windows/plugin_wrapper_32.vcxproj.filters ] ; then
	`git diff --quiet $1 $2 -- platforms/windows/plugin_wrapper.vcxproj.filters.template`
	upstream_changed=$?	# exit code

	if [ "$upstream_changed" = "1" ] && [ -e platforms/windows/plugin_wrapper.vcxproj.filters ] ; then
		echo Removing plugin_wrapper.vcxproj.filters
		rm platforms/windows/plugin_wrapper.vcxproj.filters
		run_update=1
	fi
	if [ "$upstream_changed" = "1" ] && [ -e platforms/windows/plugin_wrapper_32.vcxproj.filters ] ; then
		echo Removing plugin_wrapper_32.vcxproj.filters
		rm platforms/windows/plugin_wrapper_32.vcxproj.filters
		run_update=1
	fi
else
	run_update=1
fi

if [ -e platforms/windows/OperaMAPI.vcxproj ] || [ -e platforms/windows/OperaMAPI_32.vcxproj ] ; then
	`git diff --quiet $1 $2 -- platforms/windows/OperaMAPI.vcxproj.template`
	upstream_changed=$?	# exit code

	if [ "$upstream_changed" = "1" ] && [ -e platforms/windows/OperaMAPI.vcxproj ] ; then
		echo Removing OperaMAPI.vcxproj
		rm platforms/windows/OperaMAPI.vcxproj
		run_update=1
	fi
	if [ "$upstream_changed" = "1" ] && [ -e platforms/windows/OperaMAPI_32.vcxproj ] ; then
		echo Removing OperaMAPI_32.vcxproj
		rm platforms/windows/OperaMAPI_32.vcxproj
		run_update=1
	fi
else
	run_update=1
fi

if [ -e platforms/windows/OperaMAPI.vcxproj.filters ] || [ -e platforms/windows/OperaMAPI_32.vcxproj.filters ] ; then
	`git diff --quiet $1 $2 -- platforms/windows/OperaMAPI.vcxproj.filters.template`
	upstream_changed=$?	# exit code

	if [ "$upstream_changed" = "1" ] && [ -e platforms/windows/OperaMAPI.vcxproj.filters ] ; then
		echo Removing OperaMAPI.vcxproj.filters
		rm platforms/windows/OperaMAPI.vcxproj.filters
		run_update=1
	fi
	if [ "$upstream_changed" = "1" ] && [ -e platforms/windows/OperaMAPI_32.vcxproj.filters ] ; then
		echo Removing OperaMAPI_32.vcxproj.filters
		rm platforms/windows/OperaMAPI_32.vcxproj.filters
		run_update=1
	fi
else
	run_update=1
fi

if [ $run_update = 1 ] ; then
	echo "Generating source files..."
	python modules/hardcore/scripts/sourcessetup.py --ignore-missing-sources
	echo "Regenerating project file..."
	python platforms/windows/vcxproj_update.py
fi
