

import os, sys
import shutil

def any(iterable):
    for element in iterable:
        if element:
            return True
    return False

configuration = [
	{	'version':'02', 'target':'output'},
	{	'version':'02', 'target':'dist-output'}	
]

file_list = [
	os.path.join("modules", "pubsuffix", "documentation", "draft-pettersen-subtld-structure.txt")
	]

if any([x == "debug_build" for x in sys.argv[1:]]):
	configuration.append({	'version':'01', 'target':'output'})

for conf in configuration:
	target_path = os.path.join(conf['target'], "domains", conf['version'], "documentation");

	if not os.access(target_path, os.F_OK):
		os.makedirs(target_path)
	
	for file1 in file_list:
		shutil.copyfile(file1, os.path.join(target_path, os.path.basename(file1)))
