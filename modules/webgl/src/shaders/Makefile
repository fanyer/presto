#
# Ultra-simple..chop up BEGIN/ENDs and emit a .h from that.
#
all: create-shader

.PHONY: create-shader

SED=sed
TR=tr
CAT=cat

INPUT=hlsl-shaders.gs 
OUTPUT=wgl_cgbuilder_shaders.h
HEADER=header.txt
MIDDLE=middle.txt
POSTER=poster.txt

create-shader:
	${CAT} ${HEADER} > ${OUTPUT}
	${SED} -e 's@//.*@@g;/^$$/d;s/BEGIN \(.*\)$$/#define FUN_\1 UNI_L("/g;s/^END.*/")/g' < ${INPUT} | ${TR} -d '\r\n' | tr -s ' ' | ${SED} -e s'/")/")\n/g' >> ${OUTPUT}
	${CAT} ${MIDDLE} >> ${OUTPUT}
	${SED} -n -e 's/^BEGIN \(.*\)$$/      FUN_\1,/pg' < ${INPUT} >> ${OUTPUT}
	${CAT} ${POSTER} >> ${OUTPUT}
