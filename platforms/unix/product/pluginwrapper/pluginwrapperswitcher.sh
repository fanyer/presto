#!/bin/sh
#
# This script chooses a plugin wrapper based on the arguments given to it

wrappertype="native"
takenext=""
for arg in "$@"; do
	if [ $takenext ]; then
		wrappertype=${arg}
		break
	fi
	if [ "${arg}" = "-type" ]; then
		takenext="yes"
	fi
done

wrapper="$0"-"${wrappertype}"

exec ${OPERA_PLUGINWRAPPER_PREFIX} "$wrapper" "$@"
