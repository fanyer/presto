
import os
import sys

changelog_filename = os.path.join("..", "..", "pubsuffixlist", "changelog.txt")
if len(sys.argv) > 2:
	changelog_filename = sys.argv[2]
changelog_text = ""

try:
	file = open(changelog_filename, "r")
	changelog_text = file.read();
	file.close()
except IOError:
	print "Changelog not present"

readme_text = """\
The Opera XML Public Suffix List v1.1

This XML version of the Public Suffix list is based on the 
Mozilla Public Suffix list, with some modifications listed below.

The database is released under the same tri-license as Mozilla's list. 
See each file or the LICENSE file for details.

The XML database is originally distributed via 
http://redir.opera.com/pubsuffix .

The format of the XML files is documented in the file 
draft-pettersen-subtld-structure.txt in the documentation folder of 
this distribution.

Modifications:

 * dot-Name TLD: All domains, whatever the level, are considered
 registry-like (similar to co.uk). The reason for this is that we have
 no documention of the structure, but we know that that the second-level
 domains can be either normal domains or registry-like. Given the number
 of possible family names at the second level, it is not practical to
 have a list of registry-like domains. It is also uncertain how many
 levels down into the hierarchy such registry-like domains can be found.
 When the maximum depth has been satisfactorily documented, the entry 
 will be updated with that maximum level.
 
 * dot-us TLD: Each domain under a .us state domain, for example, 
 ca.us, can contain several registry- and non-registry-like domains.
 This list of domains is fully expanded in this version of the XML 
 Public Suffix database.

""" + changelog_text + """ 
Mozilla Public Suffix list:

The Mozilla Public Suffix home page is http://publicsuffix.org/ .

The most recent version of the original Mozilla Public Suffix list 
can be found at http://mxr.mozilla.org/mozilla-central/source/netwerk/dns/src/effective_tld_names.dat?raw=1 

Other information:

Background information about the Public Suffix List can be found at

    http://publicsuffix.org/learn/
    http://my.opera.com/yngve/blog/show.dml/267415

"""
     
