import os, sys, re	
import converter
import license
import readme

configuration = [
	{	'version':'02', 
		'body_prefix':'<?xml version="1.0" encoding="utf-8" ?>\n',
		'license':"<!--\n"+(license.long_license)+"-->\n",
		'post_processor': None}
]

filename = os.path.join("..", "..", "pubsuffixlist", "effective_tld_names.dat")
if len(sys.argv) > 1:
	filename = sys.argv[1]

converter.GenerateFiles(filename, os.path.join("dist-output", "domains"), configuration) 


for conf in configuration:
	license_file = open(os.path.join("dist-output", "domains", conf['version'], "LICENSE"),"wt")
	license_file.write(license.long_license)
	license_file.write("//\n// The Opera XML Public Suffix List contain the following files in the containing folder:\n//\n//     " + 
				"\n//     ".join([x for x in os.listdir(os.path.join("dist-output", "domains", conf['version'])) if re.match(r"^.*\.xml$", x)]) + 
				"\n//\n//  The files are available as a single download at http://redir.opera.com/pubsuffix/ ." )
	license_file.close()
	readme_file = open(os.path.join("dist-output", "domains", conf['version'], "README"),"wt")
	readme_file.write(readme.readme_text)
	readme_file.close()
