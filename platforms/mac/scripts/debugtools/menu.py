import os, sys, shutil
from TerminalController import TerminalController

term = TerminalController()

# Clear everything
sys.stdout.write(term.CLEAR_SCREEN)

action_text = "";

prefs_folders = [ os.path.expanduser("~/Library/Application Support/Opera Debug"),
					os.path.expanduser("~/Library/Caches/Opera Debug"),
					os.path.expanduser("~/Library/Preferences/Opera Debug Preferences"),
					os.path.expanduser("~/Library/Opera Debug") ]

prefs_copy_folders = [ os.path.expanduser("~/Library/Application Support/Opera Debug copy"),
						os.path.expanduser("~/Library/Caches/Opera Debug copy"),
						os.path.expanduser("~/Library/Opera Debug copy") ]

#for filename in prefs_folders:
#	print filename

def copyprefsfolder(src_folder, dest_folder):
	folders_found = 0
	for filename in src_folder:
		if os.path.exists(filename):
			folders_found = folders_found + 1

	if folders_found > 0:
		for filename in dest_folder:
			shutil.rmtree(filename, True)

	current_folder = 0
	folders_found = 0
	for filename in src_folder:
		if os.path.exists(filename):
			shutil.copytree(filename, dest_folder[current_folder])
			folders_found = folders_found + 1
		current_folder = current_folder + 1
	return folders_found


while True:
	sys.stdout.write("\n")
	sys.stdout.write("     "+term.BLUE+term.BOLD+"Mac Developer Helper Tools"+term.NORMAL+"\n")
	sys.stdout.write("\n")
	sys.stdout.write("     "+term.BOLD+"Opera debug preferences"+term.NORMAL+"\n")
	sys.stdout.write("     ========================\n")
	sys.stdout.write("     1. Delete\n")
	sys.stdout.write("     2. Save current\n")
	sys.stdout.write("     3. Restore previously saved\n")
	sys.stdout.write("\n")
	sys.stdout.write("     "+term.BOLD+"Block incoming port"+term.NORMAL+"\n")
	sys.stdout.write("     ===================\n")
	sys.stdout.write("     4. Block xml.opera.com (AutoUpdate, etc)\n")
	sys.stdout.write("     5. Block link-server.opera.com\n")
	sys.stdout.write("     6. Unblock All\n")
	sys.stdout.write("\n")
	sys.stdout.write("     "+term.BOLD+"Throttle Bandwidth"+term.NORMAL+"\n")
	sys.stdout.write("     ===================\n")
	sys.stdout.write("     7. Limit port 80 to 40KByte/s\n")
	sys.stdout.write("     8. Limit port 80 to 15KByte/s\n")
	sys.stdout.write("     9. Remove throttling\n")
	sys.stdout.write("\n")
	sys.stdout.write("\n")
	sys.stdout.write("     Select action to take (q to quit): ")

	if action_text != "":
		sys.stdout.write("\n")
		sys.stdout.write("\n")
		sys.stdout.write("     "+action_text+"\n")
		for i in range(3):
			sys.stdout.write(term.UP)
		for i in range(40):
			sys.stdout.write(term.RIGHT)


	key = raw_input()

	if key == "1":
		folders_found = 0
		for filename in prefs_folders:
			if os.path.exists(filename):
				shutil.rmtree(filename)
				folders_found = folders_found + 1

		if folders_found > 0:
			action_text = "Debug Preferences Deleted"
		else:
			action_text = "No preferences found to delete"

	elif key == "2":
		folders_found = copyprefsfolder(prefs_folders, prefs_copy_folders)

		if folders_found > 0:
			action_text = "Debug Preferences Saved"
		else:
			action_text = "No preferences found to save"

	elif key == "3":
		folders_found = copyprefsfolder(prefs_copy_folders, prefs_folders)

		if folders_found > 0:
			action_text = "Debug Preferences Restored"
		else:
			action_text = "No preferences found to restore"

	elif key == "4":
		os.system("sudo ipfw add 2001 deny ip from xml.opera.com to any in")
		action_text = "xml.opera.com blocked"

	elif key == "5":
		os.system("sudo ipfw add 2001 deny ip from link-server.opera.com to any in")
		action_text = "link-server.opera.com blocked"

	elif key == "6":
		os.system("sudo ipfw delete 2001")
		action_text = "All ports unblocked"

	elif key == "7":
		os.system("sudo ipfw pipe 1 config bw 40KByte/s")
		os.system("sudo ipfw add 1 pipe 1 src-port 80")
		action_text = "Port 80 restricted to 40KByte/s"

	elif key == "8":
		os.system("sudo ipfw pipe 1 config bw 15KByte/s")
		os.system("sudo ipfw add 1 pipe 1 src-port 80")
		action_text = "Port 80 restricted to 15KByte/s"

	elif key == "9":
		os.system("sudo ipfw delete 1")
		action_text = "Port 80 unrestricted"

	elif key == "q" or key == "Q":
		for i in range(3):
			sys.stdout.write(term.DOWN)
		break
	else:
		action_text = ""

	# Clear everything
	sys.stdout.write(term.CLEAR_SCREEN)

