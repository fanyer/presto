#!/bin/sh

set -e

cd "$(dirname $0)/.."
rm -rf gst-opera
git clone git.oslo.osa:/var/git/gstreamer/gst-opera
cd gst-opera
git submodule update --init
find -name .git | xargs rm -rf
