
import 	os, re,fnmatch
import fileinput

def get_filename_list(top, dir, pattern):
	"""Recursivly generate a listing of files in the file 
		hierarchy starting at topdir/dir and that matches the 
		regular expression string in pattern.
		
		top should be an absolute path, dir is considered a relative path, and may be empty
		
		The returned value is a dictionary with the absolute pathname as key, and the 
		name relative to the top directory as the value."""
		
	file_list = {}
	dirbase = os.path.join(top, dir)
	for dirpath, dirnames,filenames in os.walk(dirbase) :
		for f in (x for x in filenames if re.search(pattern,x, re.IGNORECASE)):
			pathname = os.path.join(dirpath, f)
			pathname_mod = os.path.basename(f)
			file_list[pathname]=pathname_mod
	return file_list

def remove_filtered(filelist, filters):
	"""return a list of the (full_filename, filename) tuples in filelist
		that does NOT match any of the entries in the filters list"""
	filter_match = set(x for x in filters if not x.find("*"))
	filter_wild = [x for x in filters if  x not in filter_match]

	temp1 =  [f for f in filelist.values() if f not in filter_match ]
	
	for filter in filter_wild:
		temp2 = fnmatch.filter(temp1, filter);
		for f in temp2:
			temp1.remove(f)
					
	return dict([(ff, f) for ff, f in filelist.items() if  f in temp1])
	
def keep_filtered(filelist, filters):
	"""return a list of the (full_filename, filename) tuples in filelist 
		that matches at least one of the entries in the filters list"""

	filter_match = set(x for x in filters if x.find("*")<0)
	filter_wild = [x for x in filters if  x not in filter_match]

	temp = set(filelist.values())
	temp1 =  temp.intersection(filter_match)
	
	temp2 = list(temp.difference(temp1))
		
	for filter in filter_wild:
		temp3 = fnmatch.filter(temp2, filter);
		for f in temp3:
			temp1.add(f)
			temp2.remove(f)
					
	return dict([(ff, f) for ff, f in filelist.items() if f in temp1] )

def process_named_files(top,dir, name, func, param):
	"""Read all files matching name (a regexp) in the top/dir hieratrchy, and return the 
		call the processfiles with func and param for the resulting list of files"""

	read_files_list = get_filename_list(top,dir, name)
	
	processfiles(read_files_list, func, param)


def processfiles(filenames, process_func, param):
	"""process the files in filenames (which is a dictionary with each entry 
		of the file's absolute path (the key) and the name relative to the project top dir.
		Each file is read and passed to the process_func along with the realtive name and the 
		provided param parameter.
	
			* filenames	:	A dictionary of Full_path:relative_path string items
			* process_func:	A function taking the arguments (rel_filename, lines, param), where filename is the current file name,
							lines is the file contents, and param is the application specific param attribute input to this function
			*param 			Opaque application specific structure, only used by process_func """
	
	for filename in filenames:
		current_file = filenames[filename]
		#print "Scanning ", current_file,"\n"
		process_func(current_file,[line for line in fileinput.input(filename)], param)

