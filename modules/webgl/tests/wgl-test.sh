#!/bin/sh
#
# Expected output testing; simple.
#
WGL=../standalone/wgl
WGL_OPTS=-v
DIFF=diff

SHOW_PASS=NO

mkdir outputs 2>/dev/null || true

processInput()
{
    INPUT=$1
    INPUT_FILE=$2
    EXTRA_OPTS=""
    if $(head -1 ${INPUT_FILE} | grep -q "^// dump_output"); then
       EXTRA_OPTS="-d";
    fi
    if $(head -1 ${INPUT_FILE} | grep -q "^// hlsl_codegen"); then
       EXTRA_OPTS="-g";
    fi
    $WGL $WGL_OPTS ${EXTRA_OPTS} $i > outputs/${INPUT_FILE}.out
    if [ ! -e ${INPUT_FILE}.exp ]; then
	if [ "$(stat -c '%s' outputs/${INPUT_FILE}.out)" -ne "0" ]; then
	   echo "${INPUT}: FAIL (expected no output)";
	   cat outputs/${INPUT_FILE}.out
	else
	   [ "x${SHOW_PASS}" = "xYES" ] && echo "$INPUT: PASS";
	fi
    else
        if $(cmp -s ${INPUT_FILE}.exp outputs/${INPUT_FILE}.out); then
	   [ "x${SHOW_PASS}" = "xYES" ] && echo "$INPUT: PASS";
	else
	    OUT_DIFF=$($DIFF -q --ignore-space-change --strip-trailing-cr --ignore-blank-lines ${INPUT_FILE}.exp outputs/${INPUT_FILE}.out)
	    if [ -z "$OUT_DIFF" ]; then
		[ "x${SHOW_PASS}" = "xYES" ] && echo "$INPUT: PASS";
	    else
		echo "${INPUT}: FAIL"
		$DIFF ${DIFF_OPTS} outputs/${INPUT_FILE}.out ${INPUT_FILE}.exp
	    fi
        fi
    fi
    return 0;
}

echo "Vertex shader tests"

for i in *.vs
do
    INPUT=$(echo $i | sed -e 's/.vs$//g')
    INPUT_FILE=$i
    processInput "$INPUT" "$INPUT_FILE"
done

WGL_OPTS="${WGL_OPTS} --oes -fs"
echo "Fragment shader tests"
for i in *.fs
do
    INPUT=$(echo $i | sed -e 's/.fs$//g')
    INPUT_FILE=$i
    processInput "$INPUT" "$INPUT_FILE"
done

