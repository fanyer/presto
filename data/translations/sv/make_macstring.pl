#!/bin/sh
perl -ne 'if (/^#: ([A-Z0-9_]+):([0-9]+)/) { print $1, "\n" }' < mac.po > mac.strings
