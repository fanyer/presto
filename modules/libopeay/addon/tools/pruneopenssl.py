
import filelist
import shutil,os,re

openssl_path = "openssl"

remove_folders=(
		"apps",
		"bugs",
		"certs",
		"demos",
		"MacOS",
		"ms",
		"Netware",
		"os2",
		"perl",
		"shlib",
		"ssl",
		"test",
		"times",
		"tools",
		"util",
		"VMS",
		"include",
		"openssl",
		os.path.join("crypto","pkcs7", "p7"),
		os.path.join("crypto","pkcs7", "t"),
		os.path.join("crypto","des", "times"),
		os.path.join("crypto","des", "t"),
		os.path.join("doc","apps"),
		os.path.join("doc","ssl")
		)
		
remove_file_patterns = (
	"makefile\.*",
	"install.*",
	".*\.com",
	"README.*",
	"VERSION",
	"FAQ",
	"config.*",
	"news",
	"openssl.doxy",
	"openssl.spec",
	"problems"
)


extra_hfiles = [
	"cryptlib.h",
	"e_os.h",
	"e_os2.h",
	"ext_dat.h",
	"md32_common.h",
	"mdc2.h",
	"o_time.h",
	"rc5.h"
	]
	
extra_to_delete_after = [
	"e_os.h",
	"e_os2.h"
	]
	
dirlist = [x for x in sorted(os.listdir(".")) if re.match("openssl", x, re.IGNORECASE)];
if dirlist:
	openssl_path = dirlist[-1]
	
files_to_copy = filelist.get_filename_list(os.path.join(openssl_path, "include", "openssl"), "",".*")

for folder in remove_folders:
	to_delete = os.path.join(openssl_path, folder)
	print "delete ", to_delete
	try:
		shutil.rmtree(to_delete);
	except:
		print "not present"

files_to_delete = {}
for pat in remove_file_patterns:
	files_to_delete.update(filelist.get_filename_list(openssl_path,"",pat))

for file in files_to_delete:
	print "delete ", file
	os.remove(file)

try:
	os.makedirs(os.path.join(openssl_path,"openssl"))
except:
	print "directory already created"

h_files =filelist.get_filename_list(openssl_path,"",".*\.h")

copy_h_files = filelist.keep_filtered(h_files, files_to_copy.values()+ extra_hfiles)

for file in copy_h_files:
	print "copy file ", file, " to openssl directory"
	shutil.copyfile(file, os.path.join(openssl_path,"openssl", os.path.basename(file)));
	
for file in extra_to_delete_after:
	print "delete ", file
	os.remove(os.path.join(openssl_path,file));
