readme for ui_interaction (aka windowcommander):

for overview-style information about how to use windowcommander, first
take a look at
http://wiki.intern.opera.no/cgi-bin/wiki/wiki.pl?Modules/ui_interaction
.

when/if you wish to implement it/use it, take a look in the file
OpWindowCommander.h in this directory.

questions can be directed at rikard@opera.com .

to compile windowcommander on a (new) platform, compile the following files:

* modules/windowcommander/src/WindowCommander.cpp
* modules/windowcommander/src/WindowCommanderManager.cpp
* modules/windowcommander/src/TransferManager.cpp

the files included in the module are *.h and *.cpp in this directory and in the src subdirectory.

for a list of defines that impact this module, take a look at the
wiki page.

