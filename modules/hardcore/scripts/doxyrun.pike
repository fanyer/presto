/* -*- tab-width: 4; indent-tabs-mode: t -*- */

string root = normalize_path("../../../");
string doxygen_exe = "";
string dot_path = "";
int timelimit = -1;

int main(int argc, array(string) argv)
{
	int starttime = gethrtime();
	int is_getting_root = 0;

    for (int i=0 ; i < argc ; i++)
    {
        if (argv[i] == "--root" && argc > i + 1)
        {
            root = argv[i + 1];
			is_getting_root = 1;
            i++;
        }
        else if (argv[i] == "--doxygen" && argc > i + 1)
        {
            doxygen_exe = argv[i + 1];
			is_getting_root = 0;
            i++;
        }
		else if (argv[i] == "--timelimit" && argc > i + 1)
		{
			timelimit = (int)argv[i + 1];
			is_getting_root = 0;
			i++;
		}
		else if (argv[i] == "--help")
		{
			write(sprintf("doxyrun.pike: usage: %s --root root_directory [--doxygen path_to_doxygen_executable] [--windows]\n", argv[0]));
			exit(1);
		}
		else if (is_getting_root == 1)
		{
			root = root + " " + argv[i];
		}
    }

	root = normalize_path(root);

    mapping paths = look_for_tools();
	if (doxygen_exe == "")
		doxygen_exe = paths["doxpath"];
	if (paths["dotpath"])
    		dot_path = dirname(paths["dotpath"]);

	root = root-"\"";

	mapping settings = check_for_settings(combine_path(root, "doxyrun.ini"));
	if (!settings)
	{
		write("failed to check settings\n");
	} else if (settings["ALL"] == "never" && sizeof(settings) == 1)
    {
        write("No documentation generated.\n");
	    return 0;
    }

	if (!doxygen_exe || doxygen_exe == "")
	{
		write("doxyrun.pike: no doxygen binary. interrupting.\n");
		return 0;
	}

    //write(sprintf("doxyrun.pike: doxygen binary: \"%s\"\n", doxygen_exe));
    //write(sprintf("doxyrun.pike: dot path: %s\n", dot_path ? "\"" + dot_path + "\"" : "sorry, couldn't find it"));


    array(string) doxyfiles = find_dox_dirs(root, settings);
	array(string) dirty_doxyfiles = get_dirty(doxyfiles, settings);

	int modules_to_process = sizeof(dirty_doxyfiles);
    foreach(dirty_doxyfiles, string file)
    {
        file = normalize_path(file);
        run_doxygen(file);
		--modules_to_process;
		if (modules_to_process > 0 && timelimit >= 0)
		{
			int time_run = (gethrtime() - starttime) / 1000;
			if (time_run > timelimit * 1000)
			{
				write(sprintf("doxyrun.pike: time limit reached, deferring %d modules (ran for %d ms).\n", modules_to_process, time_run));
				break;
			}
		}
    }

	array(string) modules = GetModules(combine_path(root, "modules"));
	modules += GetModules(combine_path(root, "adjunct"));
	modules += GetModules(combine_path(root, "platforms"));

	write_index(combine_path(root, "index.html"), sort(modules));

    write(sprintf("doxyrun.pike: done. generation took %d ms\n", (gethrtime() - starttime) / 1000));
	return 0;
}

mapping check_for_settings(string sfile)
{
	object s = file_stat(sfile);
	if (!s && !write_default_settings(sfile))
		return 0;

	object file = Stdio.File(sfile, "r");
	if (!file)
		return 0;

	mapping retmap = ([]);

	array(string) contents = (file->read() - "\r") / "\n";
	foreach(contents, string line)
	{
		int hashidx = search(line, "#");
		if (hashidx == 0)
			continue;
		else if (hashidx > 0)
			line = line[0..hashidx - 1];

		line = String.trim_all_whites(line);

		array(string) splitted = array_sscanf(line, "%[^\t ]%*[\t ]%[^\t ]");
		if (sizeof(splitted) == 2 && splitted[0] != "" && splitted[1] != "")
		{
			retmap[splitted[0]] = splitted[1];
		}
	}
	return retmap;
}

int write_default_settings(string sfile)
{
	object file = Stdio.File(sfile, "wc");
	if (!file)
		return 0;
	file->write("# format of this file:\n");
	file->write("# module_name interval\n");
	file->write("# \"#\" first on a line means comment\n");
	file->write("# the module name \"ALL\" is reserved and matches all modules (surprise!)\n");
	file->write("# valid interval values:\n");
	file->write("#  always\n");
	file->write("#  hourly\n");
	file->write("#  daily\n");
	file->write("#  weekly\n");
	file->write("#  never\n");
	file->write("\n");
	file->write("ALL never\n");
	return 1;
}

mapping look_for_tools()
{
	mapping paths = ([]);
#ifdef __NT__
	mapping env = RegGetValues(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
	//write(sprintf("PATH: %s\n", env["Path"]));
	foreach(env["Path"] / ";", string dir)
	{
		string doxpath = dir + "\\doxygen.exe";
		object s = file_stat(doxpath);
		if (s)
		{
			//write(sprintf("doxyrun.pike: found doxygen.exe in %s\n", dir));
			paths["doxpath"] = doxpath; 
		}

		string dotpath = dir + "\\dot.exe";
		s = file_stat(dotpath);
		if (s)
		{
			//write(sprintf("doxyrun.pike: found dot.exe in %s\n", dir));
			paths["dotpath"] = dotpath;
		}
	}
#else // __NT__
    paths["doxpath"] = Process.search_path("doxygen");
    paths["dotpath"] = Process.search_path("dot");
#endif // __NT__
	return paths;
}

array(string) GetModules(string dir)
{
    Stdio.File file = Stdio.File(combine_path(dir, "readme.txt"), "r");
    
    array(string) result = ({});
    array(string) lines = ({});
    lines = ((file->read() - "\r") / "\n")[*] + "\n";
    file->close();
    
    foreach (lines, string line)
    {
		if (!sizeof(line))
			continue;
		if ((line[0] >= 'a' && line[0] <= 'z') ||
			(line[0] >= 'A' && line[0] <= 'Z'))
		{
			sscanf( line, "%s%*[ \t\n]", line );
            line -= "\n";
			result += ({ combine_path(dir, line) });
		}
    }

    return sort(result);
}

array(string) find_dox_dirs(string root, mapping settings)
{
    array(string) retval = ({});
	array(string) docdirs = GetModules(combine_path(root, "modules"));
	docdirs += GetModules(combine_path(root, "adjunct"));
	docdirs += GetModules(combine_path(root, "platforms"));

	foreach(docdirs, string dir)
	{
		string docdir = combine_path(dir, "documentation");
		if (!Stdio.is_dir(docdir))
		{
		    if (Stdio.is_dir(dir) && settings["ALL"] != "never")
			write(sprintf("doxyrun.pike: module %s has no documentation directory!\n", basename(dir)));
		}
		else
		{
			array(string) files = get_dir(docdir);
			foreach(files, string file)
			{
				if (has_prefix(file, "Doxyfile"))
				{
					//write(sprintf("doxyrun.pike: found %s in %s\n", file, basename(dir)));
					retval += ({ combine_path(docdir, file) });
				}
			}
		}
	}
    return retval;
}

int isuptodate(string module, object s, mapping settings)
{
	string when = settings[module] ? settings[module] : settings["ALL"];
	int retval = 0;

	if (when == "always")
	{
		// nop
	}
	else if (when == "hourly" && s && s->mtime > time() - 3600)
	{
		retval = 1;
	}
	else if (when == "daily" && s && s->mtime > time() - 3600 * 24)
	{
		retval = 1;
	}
	else if (when == "weekly" && s && s->mtime > time() - 3600 * 24 * 7)
	{
		retval = 1;
	}
	else if (when == "never")
	{
		retval = 1;
	}
	//write(sprintf("module: %s, value %s, returning %d\n", module, when, retval));

	return retval;
}

array(string) get_dirty(array(string) dirs, mapping settings)
{
	array(string) retval = ({});
	foreach(dirs, string dirdox)
	{
		array(string) doxlines = (Stdio.File(dirdox, "r")->read() - "\r") / "\n";
		string value;

		foreach(doxlines, string line)
		{
			if (sscanf(line, "HTML_OUTPUT%*[ \t]=%s", value) == 2)
			{
				//write(sprintf("doxyrun.pike: found value: %s\n", value));
				value = String.trim_whites(value);
			}
		}
		
		string dir = dirname(dirdox);
		string indexfile = combine_path(dir, value, "index.html");
		Stdio.Stat s = file_stat(indexfile);

		string module = explode_path(dirdox)[-3];
		if (isuptodate(module, s, settings))
		{
			//write(sprintf("%s was uptodate\n", module));
			continue;
		}

		object traverse = Filesystem.Traversion(combine_path(dir, ".."));
		foreach(traverse ; string d; string file)
		{
			if(!has_suffix(file, ".c") &&
				!has_suffix(file, ".cpp") &&
				!has_suffix(file, ".h"))
				continue;

			if(s && traverse->stat()->mtime < s->mtime)
			{
				continue;
			}
			else
			{
				retval += ({ dirdox });
				break;
			}
		}
	}
	return retval;
}

array(string) filter_dox_output(string output)
{
	array(string) retval = ({});
	string match;

	foreach((output - "\r") / "\n", string line)
	{
#ifdef __NT__
		string drive;
		string file;
		int line_nr;
		string rest = "";
		int ssres = sscanf(line, "%s:%s:%d: Warning: %s", drive, file, line_nr, rest);
		if (ssres == 4)
		{
			match = sprintf("%s:%s (%d) : WARNING: %s", drive, file, line_nr, rest);
		}
		else
#endif // __NT__
			match = line;

		if (match)
			retval += ({ match + "\n" });
	}
	return retval;
}

void run_doxygen(string doxfile)
{
	int starttime = gethrtime();

    doxfile = normalize_path(doxfile);
    write(sprintf("doxyrun.pike: processing %s... \n", doxfile));

	// using regular files instead of /dev/null and NUL, because the script hangs on NT if using NUL.
    Stdio.File stdin = Stdio.File();
    Stdio.File stderr = Stdio.File("doxerr.txt", "cwt");
    Stdio.File stdout = Stdio.File("doxstd.txt", "cwt");

	mapping args = ([ "stdin" : stdin->pipe(), "stdout" : stdout, "stderr" : stderr, "cwd": dirname(doxfile) ]);

	string dot_arg;
	if (dot_path && dot_path != "")
		dot_arg = "HAVE_DOT = YES\nDOT_PATH = " + dot_path + "\nCLASS_GRAPH = YES\nCOLLABORATION_GRAPH = YES\nUML_LOOK = YES\nTEMPLATE_RELATIONS = YES\nINCLUDE_GRAPH = YES\nINCLUDED_BY_GRAPH = YES";
	else
		dot_arg = "HAVE_DOT = NO";

    Process.create_process dox = Process.create_process( ({ doxygen_exe, "-"}), args);
	string doxyfile_contents = Stdio.File(doxfile, "r")->read() - "\r";
	stdin->write(doxyfile_contents + "\n" + dot_arg + "\n");
	stdin->close();

	dox->wait();
    write(sprintf("doxyrun.pike: (%d ms)\n", (gethrtime() - starttime) / 1000));

    Stdio.File stderrfile = Stdio.File("doxerr.txt", "r");
	string errput = stderrfile->read();
	if (!errput)
		return; // we're done
	write(filter_dox_output(errput));
}

string index_preamble = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\"><html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"coredoc.css\"/><title>Opera Module Documentation</title></head><body><h1>Opera Module Documentation</h1></body>\n";
string index_postscriptum ="</body></html>";

void write_index(string file, array(string) modules)
{
    Stdio.File f = Stdio.File(file, "ctw");
    f->write(index_preamble);
    f->write(sprintf("Generated %s<p>\n", ctime(time())));
	string current_prefix = "";
	int in_list = 0;
    foreach(modules, string module)
    {
		array(string) components = explode_path(module);
		string module_path = components[-2] + "/" + components[-1];
		string module_name = components[-1];

		if (current_prefix != components[-2])
		{
			current_prefix = components[-2];
			if (in_list)
			{
				f->write("</ul>\n");
				in_list = 0;
			}
			f->write(sprintf("<h2>%s</h2>\n", current_prefix));
			f->write("<ul>\n");
			in_list = 1;
		}
		int has_index = 0 != file_stat(combine_path(module, "documentation", "index.html"));

		f->write("<li>");
		if (has_index)
			f->write(sprintf("<a href=\"%s/documentation/index.html\">%s</a>", module_path, module_name));
		else
			f->write(sprintf("%s", module_name));
		f->write("<br>\n");
    }
    f->write(index_postscriptum);
}
