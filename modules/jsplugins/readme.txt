API, CODE AND EXAMPLES FOR THE JS PLUGIN INTERFACE IMPLEMENTATION IN
OPERA 8 AND OPERA 9

people to ask questions:
lasse@opera.com jl@opera.com rikard@opera.com

but lookie here first:

SHORT SUMMARY:

to use this code, check out this module (jsplugins) under modules/ in
the opera source tree.

add the files to the compilation.

turn on the define JS_PLUGIN

more details below.

GENERAL:

unless you're compiling for u**x, you must manually make sure
that the following files are included in the compilation:

modules/jsplugins/src/js_plugin_context.cpp
modules/jsplugins/src/js_plugin_manager.cpp
modules/jsplugins/src/js_plugin_object.cpp

if you're compiling for u**x, you just have to follow the
single step right below.

u**x:

in Makefile.private, turn on

JS_PLUGIN_SUPPORT       = YES

w*ndows:

in core/vfunc.h, make sure that the following define is turned on:

#define JS_PLUGIN_SUPPORT

COMPILING THE PLUGINS

for w*ndows, there are .dsp files in the jsplugins/examples directory.
include them in the opera workspace and set the win32 debug unicode
configuration. compile. the plugins will be put in the
debug_unicode/exe/jsplugins directory, which conveniently enough is
where the js plugin manager will look for them if you have a debug
unicode opera version.

for u**x, there is a Makefile in the jsplugins/examples directory.
type make to compile all examples. currently you will have to move
them yourself to Opera/jsplugins (since on u**x you probably want to
symlink them anyway).


DISTRIBUTING THE PLUGIN KIT TO CUSTOMERS

the customer first has to sign the JSPlugin License Agreement
(template in doc/Opera_JSPlugin_API_License.sxw).

(s)he should then get following files:

jsplugin.h
doc/HOWTO.html
examples/videoplayer.c
examples/videoplayer.html
examples/jsplugin_videoplayer.dsp
examples/triggerreceiver.c
examples/triggerreceiver.html
examples/Makefile
examples/README.txt
