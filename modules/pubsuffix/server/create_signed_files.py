import sys, os, re
import signing
import converter
import license
import readme

def any(iterable):
    for element in iterable:
        if element:
            return True
    return False

configuration = [
	{	'version':'02', 
		'body_prefix':'<?xml version="1.0" encoding="utf-8" ?>\n<!-- ',
		'license':license.short_license,
		'post_processor': {'fun':signing.PerformBodySigning, 
						'data':{'keyfile':'tools/version02.key.pem', 'alg':'sha256'}}
						}
]

if any([x == "debug_build" for x in sys.argv[1:]]):
	configuration.append(
	{	'version':'01', 
		'body_prefix':'<?xml version="1.0" encoding="utf-8" ?>\n<!-- ',
		'license':license.short_license,
		'post_processor': {'fun':signing.PerformBodySigning, 
						'data':{'keyfile':'tools/key.pem', 'alg':'sha256'}}
						})

filename = os.path.join("..", "..", "pubsuffixlist", "effective_tld_names.dat")
if len(sys.argv) > 1:
	filename = sys.argv[1]

converter.GenerateFiles(filename, os.path.join("output", "domains"), configuration) 

for conf in configuration:
	license_file = open(os.path.join("output", "domains", conf['version'], "LICENSE"),"wt")
	license_file.write(license.long_license)
	license_file.write("//\n// The Opera XML Public Suffix List contain the following files in the containing folder:\n//\n//     " + 
				"\n//     ".join([x for x in os.listdir(os.path.join("output", "domains", conf['version']	)) if re.match(r"^.*\.xml$", x)]) + 
				"\n//\n//  The files are available as a single download at http://redir.opera.com/pubsuffix/ ." )
	license_file.close()
	readme_file = open(os.path.join("output", "domains", conf['version'], "README"),"wt")
	readme_file.write(readme.readme_text)
	readme_file.close()
