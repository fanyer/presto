string root;
string doxygen_exe = "doxygen";
string interpreter = "";

int main(int argc, array(string) argv)
{
    for (int i=0 ; i < argc ; i++)
    {
        if (argv[i] == "--root" && argc > i + 1)
        {
            root = System.normalize_path(argv[i + 1]);
            i++;
        }
        if (argv[i] == "--doxygen" && argc > i + 1)
        {
            doxygen_exe = argv[i + 1];
            i++;
        }
        if (argv[i] == "--windows")
        {
            interpreter = "cmd.exe /C ";
        }
    }

    if (!root)
    {
        write(sprintf("usage: %s --root root_directory [--doxygen path_to_doxygen_executable] [--windows]\n", argv[0]));
        exit(1);
    }

    write(sprintf("running from directory \"%s\"\n", root));
    write(sprintf("using doxygen binary \"%s\"\n", interpreter + doxygen_exe));

    string cwd = getcwd();

    array(string) doxyfiles = find_dox_dirs(root);

    foreach(doxyfiles, string file)
    {
        file = System.normalize_path(file);
        write(sprintf("found file %s\n", file));
        run_doxygen(file - "Doxyfile");
    }
    cd(cwd);
    write("writing index:\n");
    write_index("dox_index.html", doxyfiles);
    write("done.\n");
}

array(string) find_dox_dirs(string root)
{
    array(string) retval = ({});
    foreach(get_dir(root), string item)
    {
        //write(sprintf("%s\n", root + "/" + item));
        Stdio.Stat stat = file_stat(root + "/" + item);
        if (!stat)
        {
            write(sprintf("null stat: %s\n", item));
            continue;
        }
        if (stat->isdir)
        {
            retval += find_dox_dirs(root + "/" + item);
        }
        else if (item == "Doxyfile")
        {
            retval += ({ root + "/" + item });
        }
    }
    return retval;
}

void run_doxygen(string path)
{
    write(sprintf("running doxygen in %s\n", path));
    cd(path);
    Process.create_process dox = Process.create_process( (interpreter + doxygen_exe) / " ");
    dox->wait();
}

string index_preamble = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\"><html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"coredoc.css\"/><title>Opera Module Documentation</title></head><body><h1>Opera Module Documentation</h1></body>\n";
string index_postscriptum ="</body></html>";

void write_index(string file, array(string) doxfiles)
{
    Stdio.File f = Stdio.File(file, "ctw");
    f->write(index_preamble);
    f->write(sprintf("Generated %s<p>\n", ctime(time())));
    foreach(doxfiles, string df)
    {
        string dfpath = df - "Doxyfile";
        string module = explode_path(dfpath)[-3];
        f->write(sprintf("<a href=\"../%s/documentation/api/index.html\">%s</a><br>\n", module, module));
    }
    f->write(index_postscriptum);
}
