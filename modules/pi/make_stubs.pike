/* -*- Mode: Pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*- */

int all_lowercase = 0;
string prefix = "";

void create_output_dir()
{
	mkdir(getcwd() + "/generated_stubs");
}

int get_year()
{
    return localtime(time())["year"] + 1900;
}

string generate_copyright_header()
{
	return sprintf("/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-\n\
**\n\
** Copyright (C) 1995-%d Opera Software AS.  All rights reserved.\n\
**\n\
** This file is part of the Opera web browser.  It may not be distributed\n\
** under any circumstances.\n\
*/\n\n", get_year());
}

string get_type_with_scope(string base, string type)
{
	if (has_value(get_return_value(base, type), base + "::"))
		return base + "::" + type;
	else
		return type;
}

string get_return_value(string base, string type)
{
	if ((type / " ")[0] == "const")
		type = (type / " ")[1];

	switch (type)
	{
	case "void":
		return "";
	case "OP_STATUS":
		return "OpStatus::ERR";
	case "BOOL":
		return "FALSE";
	case "OpRect":
		return "OpRect()";
	case "OpPoint":
		return "OpPoint()";
	case "OpStringC8":
		return "OpStringC8()";
	case "CursorType":
		return "CURSOR_DEFAULT_ARROW";
	case "int":
	case "INT":
	case "long":
	case "LONG":
	case "INT32":
	case "UINT32":
	case "UINT8":
	case "UINT16":
	case "UINT":
	case "short":
	case "char":
	case "size_t":
	case "COLORREF":
	case "ShiftKeyState":
	case "unsigned int":
	case "uni_char":
	case "OpFileLength":
		return "0";
	case "double":
	case "float":
		return "0.0";
	default:    
		if (has_value(type, "*"))
			return "NULL";
		else
		{
			return sprintf("(%s::%s)0", base, type);
		}
	}
}

string get_type(string s)
{
	array(string) a = s / " ";
	string result = a[0];
	if (sizeof(a) > 1 && sizeof(a[1]) && a[1][0] == '*')
		result += "*";
	if (result == "const" || result == "unsigned")
	{
		result += " " + a[1];
		if (sizeof(a) > 2 && sizeof(a[2]) && a[2][0] == '*')
		    result += "*";
	}

	return result;
}

string get_ifdef_name(string a)
{
	string ifdef;
	if (sscanf(a, "#ifdef %s", ifdef) == 1)
		return ifdef;
	else if (sscanf(a, "#ifndef %s", ifdef) == 1)
		return "!" + ifdef;
	else
	{
		a -= "(";
		a -= ")";
		a -= "#if";
		a -= "defined";
		a = replace(a, "    ", " ");
		a = replace(a, "   ", " ");
		a = replace(a, "  ", " ");
		return String.trim_all_whites(a);
	}
}

string strip_default_param_values(string s)
{
	array(string) params = s / ",";
	for (int i=0; i < sizeof(params); ++i)
	{
		string p = params[i];
		if (has_value(p, "=") && !has_value(p, "/*"))
		{
			int add_parent = has_value(p, ")");
			params[i] = String.trim_whites(p[0..search(p, "=")-1]);
			if (add_parent)
				params[i] += ")";
		}
	}

	return params * ", ";
}

void generate_stubs(string inc, string cls, array(string) f, string file_ifdef, array(string) bi)
{
	string start = "generated_stubs/" + prefix + cls;
	if (all_lowercase)
		start = lower_case(start);

	string impclass = prefix + cls;
	string ifdef = upper_case(impclass) + "_H";

	Stdio.File h = Stdio.File(start + ".h", "wct");
	h->write(generate_copyright_header());
	h->write("#ifndef %s\n", ifdef);
	h->write("#define %s\n\n", ifdef);

	if (sizeof(file_ifdef))
	{
		h->write("%s\n\n", file_ifdef);
	}

	h->write("#include \"%s\"\n\n", inc);
	h->write("class %s : public %s\n", impclass, cls);
	h->write("{\n");
	h->write("public:\n");
	for (int i=0; i < sizeof(f); ++i)
	{
		string func = f[i];
		string b = bi[i];
		int last_ifdef_equal = i && bi[i-1] == b;
		int next_ifdef_equal = (i != sizeof(f)-1) && bi[i+1] == b;
		if (sizeof(b) && !last_ifdef_equal)
			h->write("\n%s\n", b);
		h->write("\t%s;\n", func);
		if (sizeof(b) && !next_ifdef_equal)
			h->write("#endif // %s\n\n", get_ifdef_name(b));
	}
	h->write("};\n\n");

	if (sizeof(file_ifdef))
	{
		h->write("#endif // %s\n\n", file_ifdef);
	}

	h->write("#endif // !%s\n", ifdef);
	h->close();

	Stdio.File cpp = Stdio.File(start + ".cpp", "wct");
	cpp->write(generate_copyright_header());
	cpp->write("#include \"core/pch.h\"\n");

	if (sizeof(file_ifdef))
	{
		cpp->write("\n%s\n\n", file_ifdef);
	}

	cpp->write("#include \"%s\"\n", (start / "/")[1]+ ".h");
	for (int i=0; i < sizeof(f); ++i)
	{
		string func = f[i];
		string b = bi[i];
		string type = get_type(func);
		string rest = func[strlen(type)+1..];
		rest = String.trim_whites(rest);
		int last_ifdef_equal = i && bi[i-1] == b;
		int next_ifdef_equal = (i != sizeof(f)-1) && bi[i+1] == b;
		cpp->write("\n");
		if (sizeof(b) && !last_ifdef_equal)
			cpp->write("%s\n", b);
		cpp->write("%s\n", get_type_with_scope(cls, type));
		cpp->write("%s::%s\n", impclass, strip_default_param_values(rest));
		cpp->write("{\n");
		cpp->write("\tOP_ASSERT(!\"%s is not implemented.\");\n", impclass + "::" + (rest / "(")[0] + "(...)");
		if (type != "void")
			cpp->write("\treturn %s;\n", get_return_value(cls, type));
		cpp->write("}\n");
		if (sizeof(b) && !next_ifdef_equal)
			cpp->write("#endif // %s\n", get_ifdef_name(b));
	}

	if (sizeof(file_ifdef))
	{
		cpp->write("\n#endif // %s\n", get_ifdef_name(file_ifdef));
	}

	cpp->close();
}

void generate_interface(string file)
{
	if (has_suffix(file, "OpForms.h") || has_value(file, "Thunder"))
		return;

	create_output_dir();
	Stdio.File f = Stdio.File(file, "r");
	array(string) lines = (f->read() - "\r") / "\n";
	f->close();

	// Quick check to see if this file contains virtual and = 0 somewhere
	int ok = 0;
	foreach(lines;; string line)
	{
		if (sscanf(line, "%*[ \t]virtual%*[ \t]%*s=%*[ \t]0;") == 4)
		{
			ok = 1;
			break;
		}
	}
	if (!ok)
	{
		return;
	}

	string file_ifdef = "";
	string block_ifdef = "";
	string current_class = "";
	array(string) functions = ({});
	array(string) block_ifdefs = ({});
	int in_else = 0;
	for (int i=0; i < sizeof(lines); ++i)
	{
		string line = lines[i];
		string s;
		if (sscanf(line, "class %s", s) == 1 && !has_suffix(s, ";") && !has_suffix(s, "Listener") && !has_suffix(s, "Observer") && !has_value(s, ":"))
		{			
			current_class = String.trim_whites(s);
		}

		string ifdef;
		if (sscanf(line, "#if%*s%*[ \t]%s", ifdef) == 3 && !has_suffix(ifdef, "_H"))
		{
			if (!sizeof(current_class))
				file_ifdef = line;
			else
			{
				block_ifdef = line;
			}
		}

		if (has_prefix(line, "#else"))
			in_else = 1;

		if (has_prefix(line, "#endif"))
		{
			if (!sizeof(current_class))
				file_ifdef = "";
			else
			{
				block_ifdef = "";
			}
			in_else = 0;
		}

		if (!sizeof(current_class) || in_else)
			continue;

		string func;
		if (sscanf(line, "%*[ \t]virtual%*[ \t]") == 2 && !has_value(line, "{") && !has_value(line, "~"))
		{
			int add=0;
			int added = 0;
			while (!added)
			{
				if (sscanf(line, "%*[ \t]virtual%*[ \t]%s=0;", func) == 3 ||
					sscanf(line, "%*[ \t]virtual%*[ \t]%s =0;", func) == 3 ||
					sscanf(line, "%*[ \t]virtual%*[ \t]%s = 0;", func) == 3)
				{
					functions += ({ String.trim_all_whites(replace(func, "\t", " ")) });
					block_ifdefs += ({ block_ifdef });
					added = 1;
				}
				else
				{
					++add;
					line += "\n" + lines[i+add];
					if (has_value(line, "{"))
					{
						string a = String.trim_all_whites(replace(line[search(line, "virtual")+8..search(line, "{")-1], "\t", " "));
						functions += ({ a });
						block_ifdefs += ({ block_ifdef });
						break;
					}
				}
			}
			i += add;
		}

		if (has_prefix(line, "};"))
		{
			if (sizeof(functions))
			{
				generate_stubs("modules/pi" + file - getcwd(), current_class, functions, file_ifdef, block_ifdefs);
			}
			current_class = "";
			functions = ({});
			block_ifdefs = ({});
		}
	}
}

void generate_interfaces(string dir)
{
	array(string) files = get_dir(dir);
	foreach(files;; string file)
	{
		string path = dir + "/" + file;
		if (has_suffix(file, ".h"))
		{
			generate_interface(path);
		}
		else if (Stdio.is_dir(path))
		{
			generate_interfaces(path);
		}
	}
}

int main(int argc, array(string) argv)
{
	if (argc != 2 && argc != 3)
	{
		write("Usage:\n");
		write("    pike make_stubs.pike [prefix] [flags]\n");
		write("\n");
		write("prefix - Prefix to use infront of all interfaces.\n");
	    write("         Example: prefix Linux gives LinuxOpView.cpp etc\n");
		write("\nFlags:\n");
		write("    -lowercase - Makes all output filenames lowercase.\n");
		write("\n");
		write("Generated stubs will end up in ./generated_stubs/\n");
		return 0;
	}

	prefix = argv[1];
	if (argc > 2 && argv[2] == "-lowercase")
		all_lowercase = 1;

	generate_interfaces(getcwd());

	write("The stubs have been put in generated_stubs/\n");
	return 0;
}
