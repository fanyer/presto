string module_prefix = "MODULES_UTIL_";

int main(int c, array(string) v)
{
    for (int i=1; i < c; ++i)
    {
	rewrite_file(v[i]);
    }
}

int get_year()
{
    return localtime(time())["year"] + 1900;
}

int is_whitespace(string s)
{
    for (int i=0; i < sizeof(s); ++i)
    {
	if (s[i] != ' ' && s[i] != '\t')
	{
	    return 0;
	}
    }

    return 1;
}

int parse_c_comment(array(string) strings, int i)
{
    int i_start = i;
    string line = strings[i];
    if (sscanf(line, "%*[ \t]/*%*s") == 2)
    {
	for(; i < sizeof(strings); ++i)
	{
	    line = strings[i];
	    if (sscanf(line, "%*s*/%*s") == 2)
	    {
		return i - i_start;
	    }
	}
    }
    
    return -1;
}

int is_cpp_file(string file)
{
    return (search(file, ".cpp") == sizeof(file) - 4);
}

int is_h_file(string file)
{
    return (search(file, ".h") == sizeof(file) - 2);
}

string get_nice_print_name(string file)
{
    if (file[0] == '.' && file[1] == '/')
	return file[2..];

    return file;
}

string convert_file_to_ifdef(string file)
{
    string result = "";
    int i=0;
    if (file[0] == '.' && file[1] == '/')
	file = file[2..];
    file = upper_case(file);
    file = replace(file, "/", "_");
    file = replace(file, ".", "_");    

    return module_prefix + file;
}

enum pch_result
{
    PCH_NOT_FOUND=-2, 
    PCH_OK=-1
};

int check_pch(array(string) strings, int comments_end)
{
    for (int i=comments_end; i < sizeof(strings); ++i)
    {
	if (is_whitespace(strings[i])) continue;

	int d = parse_c_comment(strings, i);
	if (d > -1)
	{
	    i += d;
	    continue;
	}

	if (sscanf(strings[i], "%*[ \t]//%*s") == 2)
	{
	    continue;
	}
	
	if (strings[i] == "#include \"core/pch.h\"")
	{
	    return PCH_OK;
	}

	return i;
    }

    return PCH_NOT_FOUND;
}

array(int) find_h_ifdefs(array(string) strings, string file, int comments_end)
{
    int found_starting = 0;
    for (int i=comments_end; i < sizeof(strings); ++i)
    {
	if (is_whitespace(strings[i])) continue;

	int d = parse_c_comment(strings, i);
	if (d > -1)
	{
	    i += d;
	    continue;
	}

	if (sscanf(strings[i], "%*[ \t]//%*s") == 2)
	{
	    continue;
	}

	string define;
	if (sscanf(strings[i], "#ifndef %s", define) == 1)
	{
	    found_starting = i;
	    break;
	}
	else
	{
	    write("%s: No matching #ifndef/#endif pair found.\n", get_nice_print_name(file));
	    return ({});
	}
    }

    if (found_starting)
    {
	for (int i=sizeof(strings)-1; i > found_starting; --i)
	{
	    if (sscanf(strings[i], "#endif%*[ \t]%*s") == 2)
	    {
		return ({found_starting, i});
	    }
	}
    }

    return ({});
}

int rewrite_file(string file)
{
    Stdio.File f = Stdio.File(file, "r");
    
    array(string) strings = (f->read() - "\r") / "\n";

    f->close();

    int comments_end_on_row = 0;

    for (int i=0; i < sizeof(strings); ++i)
    {
	string line = strings[i];

	// Look for start comment
	int d = parse_c_comment(strings, i);
	if (d > -1)
	{
	    comments_end_on_row = i + d + 1;
	    break;
	}
	else if (sscanf(line, "%*[ \t]//%*s") == 2)
	{
	    break;
	}
	else
	{
	    if (!is_whitespace(line))
	    {
		break;
	    }
	    ++comments_end_on_row;
	}
    }

    array(int) ifdef_start_end = ({});
    if (is_cpp_file(file))
    {
	int res = check_pch(strings, comments_end_on_row);
	
	if (res == PCH_NOT_FOUND)
	{
	    write("%s: core/pch.h not found.\n", get_nice_print_name(file));
	}
	else if (res != PCH_OK)
	{
	    write("%s:%d: core/pch.h not included first.\n", get_nice_print_name(file), res+1);
	}
    }
    else if (is_h_file(file))
    {
	ifdef_start_end = find_h_ifdefs(strings, file, comments_end_on_row);
    }

    Stdio.File new_file = Stdio.File(file, "wt");

    new_file->write("/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-\n");
    new_file->write("**\n");
    new_file->write("** Copyright (C) 1995-%d Opera Software AS.  All rights reserved.\n", get_year());
    new_file->write("**\n");
    new_file->write("** This file is part of the Opera web browser.  It may not be distributed\n");
    new_file->write("** under any circumstances.\n");
    new_file->write("*/\n");

    for (int i=comments_end_on_row; i < sizeof(strings); ++i)
    {
	if (i != sizeof(strings) - 1 || strlen(strings[i]))
	{
	    if (sizeof(ifdef_start_end) && ifdef_start_end[0] == i)
	    {
		new_file->write("#ifndef %s\n", convert_file_to_ifdef(file));
		new_file->write("#define %s\n", convert_file_to_ifdef(file));
		++i;
	    }
	    else if (sizeof(ifdef_start_end) && ifdef_start_end[1] == i)
	    {
		new_file->write("#endif // !%s\n", convert_file_to_ifdef(file));
	    }
	    else
	    {
			int j;
			multiset whites = (<' ','\t'>);
			for (j=1; j < sizeof(strings[i]); ++j)
			{
				if (!whites[strings[i][-j]])
					break;
			}
			strings[i] = strings[i][0..sizeof(strings[i])-j];
			new_file->write("%s\n", strings[i]);
	    }
	}
    }
    new_file->close();

    return 0;
}
