#!/bin/bash
# Script for doing fixating markup on the grammar and copying it to kaleidoscope

DEST_DIR=services-stp-0
REMOTE_PATH=/var/www/html/scope

cd ../documentation
../utils/markup.py
rm $DEST_DIR/*
mkdir -p $DEST_DIR
cp fixed/* $DEST_DIR
cp ../../coredoc/coredoc.css $DEST_DIR
cd $DEST_DIR
rm .*
rm Doxyfile
rm -f *.bak
cd ..

# Now $DEST_DIR contains everything that should be on kaleidoscope
ssh kaleidoscope.vlab.osa "rm -rf $REMOTE_PATH/$DEST_DIR"
scp -r $DEST_DIR kaleidoscope.vlab.osa:$REMOTE_PATH
ssh kaleidoscope.vlab.osa "chgrp -R www-data $REMOTE_PATH/$DEST_DIR; chmod -R g+rw $REMOTE_PATH/$DEST_DIR; chmod g+x $REMOTE_PATH/$DEST_DIR"

cd ../utils
