#!/bin/sh

INSTS=`cat $1 | grep -v -e '^//' | wc -l`
NON_TEXLD=`cat $1 | grep -v -e '^//' | grep -v texld | wc -l`

echo "Total instructions: $[$INSTS - 1]"
echo "Arithmetic instructions: $[$NON_TEXLD - 1]"
echo "Texture loads: $[$INSTS - $NON_TEXLD]"