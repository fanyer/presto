#!/bin/bash
#
# Update homes with new files from this 'scope' tree

# Go to modules/scope directory
cd `dirname $0`/..

ssh homes rm -f public_html/modules/scope/documentation/*
scp documentation/*.html \
    documentation/*.txt \
    homes:public_html/modules/scope/documentation/
scp dist/*/*.jar homes:public_html/modules/scope/dist/
