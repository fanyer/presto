/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
import Stdio;
import String;

/**
* Simple testsuite creation, generates index.html in every directory below given in-dir.
*/

string CreateHTMLForFile(string basedir)
{
  array(string) files = get_dir(basedir);
  string result = "";
  array(string) dirs = ({});

  foreach (files, string file)
  {
	string absfile = basedir + "/" + file;
	if(Stdio.is_dir(absfile))
	{
		dirs += ({file});
		CreateHTML(absfile);
	}
	else if(has_suffix(file, ".svg"))
	{
		result += "\t\t\t<tr>\n\t\t\t\t<td><img width=\"320\" height=\"240\" src=\"" + replace(file, ".svg", ".png") + "\"></td>\n";
		result += "\t\t\t\t<td><object data=\"" + file + "\"></object></td>\n";
		result += "\t\t\t\t<td><a href=\"" + file + "\">" + file + "</a></td>\n\t\t\t</tr>\n";
	}
	else if(!has_suffix(file, ".png"))
	{
		Stdio.recursive_rm(absfile);
	}
  }

  string header = "";
  dirs = Array.sort_array(dirs);
  foreach(dirs, string dir)
  {
	  header += "\t\t\t<tr><td><a href=\"" + dir + "\">" + dir +
		        "</a></td></tr>\n";
  }
  
  return header + result;
}

void CreateHTML(string base_dir)
{
	string data = CreateHTMLForFile(base_dir);
	if(sizeof(data) > 0)
	{
		FILE out = FILE(combine_path(base_dir, "index.html"), "wc");
		out->write("<html>\n\t<body>\n\t\t<table>\n");
		out->write(data);
		out->write("\t\t</table>\n\t</body>\n</html>");
	}
}

/**
 * The main function. The entry point.
 */
int main(int argc, array(string) argv)
{
	int ok = 1;
	if(argc != 2)
	{
		ok = 0;
	}
	else
	{
		string base_dir = argv[1];
		if(Stdio.exist(base_dir))
		{
			CreateHTML(base_dir);
		}
		else
		{
			Stdio.stdout.write("\nPath not found: " + argv[1]);
			ok = 0;
		}
	}

	if(!ok)
	{
		Stdio.stdout.write("\nUsage: " + basename(argv[0]) + " <dirpath>");
	}
	return 0;
}
