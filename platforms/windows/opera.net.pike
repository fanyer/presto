/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
import Parser.XML.Tree;
import Stdio;
import String;

#define TRUE 1
#define FALSE 0

#if __VERSION__ < 7.5
int q = write(#"
===========================================================================
 Pike 7.4 contains a bug in the XML parser corrupting the project file. You
 should upgrade to 7.6 or newer.
 To get pike, go to http://pike.ida.liu.se
 Even numbered minor version releases are stable versions,
 odd are unstable
===========================================================================

");

dsahjdhasjkd; /* This will abort the script hopefully */
#endif

/**
 * Returns the first word on the line as the file name.
 */
string extractModuleFile(string module_sources_line)
{
	int index = search(module_sources_line, "#");
	if (index != -1)
		module_sources_line = trim_all_whites(module_sources_line[..index-1]);
	// The first word (up to the first space, if any)
	return (module_sources_line/" ")[0];
}

/**
 * Parses all properties and returns a mapping with them, or an empty
 * mapping otherwise.
 */
mapping(string:string) extractModuleFileProperties(string module_sources_line)
{
	string prefix;
	string prop_string;
	int parsed_count = sscanf(module_sources_line, "%s[%s]", prefix, prop_string);
	mapping(string:string) result = ([]);
	if (parsed_count >= 2)
	{
		// Found properties
		array(string) props_strings = prop_string/";";
		for (int i= 0; i < sizeof(props_strings); i++)
		{
			string value = "1";
			array(string) propval = props_strings[i]/"=";
			string prop_name = propval[0];
			if (sizeof(propval) > 1)
				value = propval[1];
			result[prop_name] = value;
		}
	}
	return result;
}

array(string) GetModules(string module_dir)
{
	//	string module_dir = combine_path(base, "modules")
  array(string) module_list = get_dir(module_dir);

  FILE file = FILE(combine_path(module_dir, "readme.txt"), "r");

  array(string) result = ({});
  array(string) lines = ({});
  lines = (file->read()/"\n")[*]+"\n";

  foreach (lines, string line)
    {
      if ((line[0] >= 'a' && line[0] <= 'z') ||
	  (line[0] >= 'A' && line[0] <= 'Z'))
	{
	  sscanf( line, "%s%*[ \t\n\r]", line );
	  result += ({ line });

	  // Try to find the directory.
	  if (!has_value(module_list, line))
	    {
	      werror("Module " + line + " is not in your module dir\n");
	    }
	}
    }

  return result;
}

/**
 * Writes the tree to a new file, compares it to the current file
 * and replaces the current file if the new file is different.
 *
 * @returns 0 if the old file is kept, 1 if a new file is installed.
 */
int(0..1) writeNewProjFile(string base, string proj_file_name, Node xml_node)
{
	Buffer buf = Buffer();

	// Get the xml header. (Not needed but reduces the diff.)
	FILE readheader_file = FILE(combine_path(base, proj_file_name), "r");
	object header_iterator = readheader_file->line_iterator(1);
	buf->add(header_iterator->value());
	buf->add("\r\n");
	readheader_file->close();


	if( mixed err = catch {
		vcproj_render_xml(buf, xml_node, text_quote, attribute_quote, "");
	} )
	{
		werror("writeNewProjFile: vcproj_render_xml() FAILED!\n" + describe_backtrace(err));
		exit(1);
	}

	if( mixed err = catch {
		FILE file = FILE(combine_path(base, proj_file_name+".tmp"), "wtc");
		string xml_string = buf->get();
		file->write(xml_string);
		file->close();
	} )
	{
		werror("writeNewProjFile: could not write to temporary file!\n" + describe_backtrace(err));
		exit(1);
	}

	if( mixed err = catch {
		if (read_file(combine_path(base, proj_file_name)) !=
			read_file( combine_path(base, proj_file_name+".tmp") ) )
		{
			write( proj_file_name+": writing new version of file.\n" );
//			cp(combine_path(base, proj_file_name),
//			   combine_path(base, proj_file_name+".bak"+random(1213134)));
			cp(combine_path(base, proj_file_name+".tmp"),
			   combine_path(base, proj_file_name));
			//write("copying of file content in writeNewProjFile() went ok\n");
			return TRUE;
		}
	} )
	{
		werror("writeNewProjFile(): Opera4.vcproj: could not write to new vcproj-file!\n"+
			   describe_backtrace(err));
		exit(1);
	}
	return FALSE;
}

/**
 * Finds the first child with the specifed string in the "Name"
 * attribute. If no such file is found, then an error is triggered.
 */
Node getChildWithNameAttr(Node parent, string name_string)
{
	Node child = getChildWithNameAttrNoError(parent, name_string);
	if (!child)
		error("No child with name "+name_string+"\n");
	return child;
}

/**
 * Finds the first child with the specifed string in the "Name"
 * attribute. If no such file is found, then 0 is returned.
 */
Node getChildWithNameAttrNoError(Node parent, string name_string)
{
	//  write("parent: %O", parent);
	array(Node) children = parent->get_elements();
	for (int i = 0; i < sizeof(children); i++)
	{
		Node child = children[i];
		mapping attrs = child->get_attributes();
		string name_attr = attrs["Name"];
		//write("Found tag with name %O when looking for %s\n",
		// name_attr, name_string);
		if (name_attr == name_string)
			return child;
	}
	return 0;
}


/**
 * Locates and returns the node for "Source Files". If it doesn't
 * already exists, it's created.
 */
Node getSourceFilesNode(Node root_node)
{
	Node vis_node = root_node->get_elements("VisualStudioProject")[0];
	Node files_node = vis_node->get_elements("Files")[0];
	// Locate modules node
	Node source_files = getChildWithNameAttr(files_node, "Source Files");
	if (!source_files)
	{
		mapping attrs = ([ "Name" : "Source Files" ]);
		source_files = Node(XML_ELEMENT, "Filter", attrs, "");
		write("Adding \"Source Files\" group\n");
		files_node->add_child(source_files);
	}

	return source_files;
}

/**
 * Calculates a string describing the current node in directory terms.
 * Can be used for informational messages.
 */
string getGroupNodePath(Node node)
{
	if (node->get_parent())
	{
		string name = node->get_tag_name();

		if (name == "Filter")
			name = node->get_attributes()["Name"];

		return getGroupNodePath(node->get_parent()) + "/" + name;
	}

	return "";
}

/**
 * Finds a node representing a certain group name. If it doesn't
 * exist, then it's created.
 */
Node getGroupNode(Node parent_group_node, string group_name)
{
	//  write("\n\n\tLooking for group %s in %s\n\n", group_name, getGroupNodePath(parent_group_node));
	Node group = getChildWithNameAttrNoError(parent_group_node, group_name);
	if (!group)
	{
		mapping attrs = ([ "Name" : group_name ]);
		group = Node(XML_ELEMENT, "Filter", attrs, "");
		write("Adding group '%s' to %s\n",
			  group_name,
			  getGroupNodePath(parent_group_node));
		//      write("Length before %d\n", sizeof(parent_group_node->get_elements()));
		group = parent_group_node->add_child(group);
		//      write("Length after %d\n", sizeof(parent_group_node->get_elements()));

		if (!getChildWithNameAttrNoError(parent_group_node, group_name))
		{
			error("Can not find added group\n");
		}
	}
	return group;
} //getGroupNode()


/**
 * Checks if the module is a special one that shouldn't be modified by
 * this script.
 */
int isDisabledModule(string module_name)
{
	int(0..1) is_core_1 = FALSE;
	return
		(is_core_1 && module_name == "expat") ||
		(is_core_1 && module_name == "libssl") ||
		(is_core_1 && module_name == "zlib") ||
		(is_core_1 && module_name == "lea_malloc") ||
		module_name == "coredoc" ||
		module_name == "libopenpgp";
}

/**
 * Takes an absoule path for a file in a module directory and
 * returns a relative path, relative to the directory where
 * the project file is.
 */
string getRelativeFileName(string full_name, string stop_marker,
						   int(0..1) include_stop_marker)
{
	array(string) parts = full_name / "/";
	int i = sizeof(parts);
	// 	while (parts[--i] != "modules")
	while (parts[--i] != stop_marker)
	{
	}

	if (!include_stop_marker)
	{
		i++;
	}
	// parts[i] == "modules"
	return ".\\" + parts[i..]*"\\";
}

/**
 * Removes the "Seen" marker from a node.
 */
void unmarkFileNode(Node node)
{
	if (node->get_attributes())
		m_delete(node->get_attributes(), "SeenInModuleSources");
}

/**
 * Marks a node as "Seen" so that we can remove nodes no longer
 * referenced from module.sources.
 */
void markAsSeen(Node node)
{
	node->get_attributes()["SeenInModuleSources"] = "Oui";
}

string getPchSetting(string file_name, mapping(string:string) file_props)
{
	// Precompiled headers
	// 					 Constant	Value
	// 						 pchNone	0
	// 						 pchCreateUsingSpecific	1
	// 						 pchGenerateAuto	-- removed in VS2005
	// 						 pchUseUsingSpecific	2
	string expected_pch_value = "2";
	if (has_suffix(file_name, ".c"))
	{
		// This is a workaround until we're all C++,
		// or till we've fixed the problem with mixed
		// pch files.
		expected_pch_value = "0";
	}
	else if (file_props["winpch"] != 0)
	{
		expected_pch_value = "1";
	}
//	Apparently not needed to support this anymore.
	else if (file_props["winnopch"] != 0)
	{
		expected_pch_value = "0";
	}

	return expected_pch_value;
}

/**
 * Returns TRUE if it changes anything.
 */
int(0..1) syncFileProperties(Node file_node, mapping(string:string) file_properties, array(string) no_warning_error_files, array(string) compile_for_speed_files)
{
	int(0..1) done_change = FALSE;
	array(Node) all_configurations = file_node->get_children();
	foreach(all_configurations, Node configuration)
	{
		if (configuration->get_tag_name() == "FileConfiguration")
		{
			array(Node) config_children = configuration->get_children();
			foreach(config_children, Node potential_tool_node)
			{
				if (potential_tool_node->get_tag_name() == "Tool")
				{
					// Precompiled headers
					// 					 Constant	Value
					// 						 pchNone	0
					// 						 pchCreateUsingSpecific	1
					// 						 pchGenerateAuto	-- removed in VS2005
					// 						 pchUseUsingSpecific	2
 					string expected_pch_value = getPchSetting(file_node->get_attributes()["RelativePath"],
															  file_properties);
					mapping attrs = potential_tool_node->get_attributes();
					if (attrs["Optimization"] == "2")
					{
						expected_pch_value = "0";
					}
					if (expected_pch_value == "2")		//2 is the default project setting
					{
						m_delete(attrs, "UsePrecompiledHeader");	//remove if present
					}
					else if (attrs["UsePrecompiledHeader"] != expected_pch_value)
					{
						attrs["UsePrecompiledHeader"] = expected_pch_value;
						done_change = TRUE;
						write("Setting UsePrecompiledHeader to "+expected_pch_value+" on "+file_node->get_attributes()["RelativePath"]+"\n");
					}

					if (expected_pch_value == "0" && attrs["PreprocessorDefinitions"] != "ALLOW_SYSTEM_INCLUDES")
					{
							attrs["PreprocessorDefinitions"] = "ALLOW_SYSTEM_INCLUDES";
							done_change = TRUE;
					}
					
					if(file_properties["winnopgo"] != 0)
					{
						attrs["WholeProgramOptimization"] = "false";
					}
					else
					{
						m_delete(attrs, "WholeProgramOptimization");
					}

					// Any other property we should add here?
				}
			}
		}
	}

	return done_change;
}

/**
 * With a node, a file name and a part of a file name this function
 * makes sure that the file is mentioned in the project.
 */
int syncFile(Node group_node, string sub_file_name, string full_file_name,
			 mapping(string:string) file_properties,
			 string top_dir_name, int(0..1) include_top_dir_in_paths,
			 array(string) no_warning_error_files, array(string) compile_for_speed_files)
{
	array(string) parts = sub_file_name / "/";
	if (sizeof(parts) > 1)
	{
		string group = parts[0];

		if (group != "src")
		{
			group_node = getGroupNode(group_node, group); //only call to getGroupNode
		}

		return syncFile(group_node, parts[1..]*"/", full_file_name,
						file_properties,
						top_dir_name, include_top_dir_in_paths, no_warning_error_files, compile_for_speed_files);
	}

	// Locate file
	string rel_name = getRelativeFileName(full_file_name, top_dir_name, include_top_dir_in_paths);
	rel_name = "..\\..\\" + rel_name; // The project is in platforms/windows
	array(Node) files = group_node->get_elements("File");
	for (int i = 0; i < sizeof(files); i++)
	{
		if (files[i]->get_attributes()["RelativePath"] == rel_name)
		{
			markAsSeen(files[i]);
			return syncFileProperties(files[i], file_properties, no_warning_error_files, compile_for_speed_files);
		}
	}

	// Add
	write ("Adding file %s to %s\n",
		   full_file_name,
		   getGroupNodePath(group_node));
	mapping attrs = ([ "RelativePath" : rel_name ]);
	Node file_node = Node(XML_ELEMENT, "File", attrs, "");
	addConfigurationsToFileNode(file_node, file_properties, no_warning_error_files, compile_for_speed_files);
	markAsSeen(file_node);
	group_node->add_child(file_node);
	// Add dummy text child to prevent the file from being collapsed
	Node dummy_text = Node(XML_TEXT, "", UNDEFINED, "");
	file_node->add_child(dummy_text);
	return TRUE;
}

/**
 * This adds configuration to new file elements so that they're
 * included when build.
 */
void addConfigurationsToFileNode(Node file_node,
								 mapping(string:string) file_configuration,
								 array(string) no_warning_error_files, array(string) compile_for_speed_files)
{
	addConfigurationToFileNode(file_node, "Release Unicode|Win32", TRUE /*optimized*/, file_configuration, no_warning_error_files, compile_for_speed_files, FALSE);
	addConfigurationToFileNode(file_node, "Debug Unicode|Win32",  FALSE /*not optimized*/, file_configuration, no_warning_error_files, compile_for_speed_files, TRUE);
	addConfigurationToFileNode(file_node, "Instrumented Unicode DLL|Win32",  TRUE/*optimized*/, file_configuration, no_warning_error_files, compile_for_speed_files, FALSE);
	addConfigurationToFileNode(file_node, "Release PGO Unicode DLL|Win32",  TRUE/*optimized*/, file_configuration, no_warning_error_files, compile_for_speed_files, FALSE);
	addConfigurationToFileNode(file_node, "Tagging|Win32",  FALSE /*not optimized*/, file_configuration, no_warning_error_files, compile_for_speed_files, FALSE);
	addConfigurationToFileNode(file_node, "Debug Unicode - no warning errors|Win32",  FALSE /*not optimized*/, file_configuration, no_warning_error_files, compile_for_speed_files, FALSE);
}

/**
 * This adds a configuration to a file node, based on input and the
 * contents of the file_configuration mapping.
 */
void addConfigurationToFileNode(Node file_node, string config_name,
								int(0..1) optimize,
								mapping(string:string) file_configuration,
								array(string) no_warning_error_files, array(string) compile_for_speed_files,
								int(0..1) use_WX)
{
	mapping config_attrs = ([ "Name" : config_name ]);

	string pch_setting = getPchSetting(file_node->get_attributes()["RelativePath"],
					   file_configuration);

	mapping tool_attrs = ([ "Name" : "VCCLCompilerTool" ]);

	if (pch_setting == "0")
	{
		tool_attrs["PreprocessorDefinitions"] = "ALLOW_SYSTEM_INCLUDES";
	}
	if (pch_setting != "2")		//2 is project setting
	{
		tool_attrs["UsePrecompiledHeader"] = pch_setting;
	}

	if (optimize)
	{
		int (0..1) optimize_speed = FALSE;
		for (int i = 0; i < sizeof(compile_for_speed_files); i++)
			if (search(file_node->get_attributes()["RelativePath"], compile_for_speed_files[i]) != -1)
			{
				optimize_speed = TRUE;
				break;
			}

		if (optimize_speed)
		{
			tool_attrs["Optimization"] = "2";
			tool_attrs["UsePrecompiledHeader"] = "0";
			tool_attrs["PreprocessorDefinitions"] = "ALLOW_SYSTEM_INCLUDES";
		}
	}

	if(file_configuration["winnopgo"] != 0)
	{
		tool_attrs["WholeProgramOptimization"] = "false";
	}

	if (sizeof(tool_attrs)>1)
	{
		Node config_node = Node(XML_ELEMENT, "FileConfiguration",
								config_attrs, "");
	
		Node tool_node = Node(XML_ELEMENT, "Tool", tool_attrs, "");
	
		config_node->add_child(tool_node);
		file_node->add_child(config_node);
	}
}

/**
 * This function checks if a node can/should be removed. If it's
 * not marked (and is a c/cpp file) then it should be removed.
 */
int(0..1) isUnmarkedFileNode(Node node)
{
	if (node->get_tag_name() == "File")
	{
		mapping attrs = node->get_attributes();
		string path = attrs["RelativePath"];
		if (has_suffix(path, ".cpp") || has_suffix(path, ".c"))
		{
			//          write("%s has SeenInModuleSources set to %O\n",
			//          path, attrs["SeenInModuleSources"]);
			if (attrs["SeenInModuleSources"] != "Oui")
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/**
 * Returns the file name without path for a certain node.
 */
string getFileName(Node file_node)
{
	string rel_path = file_node->get_attributes()["RelativePath"];
	string last_part = (rel_path/"\\")[-1];
	return last_part;
}

/**
 * Checks if there are files or sub filters in a node.
 */
int(0..1) hasContent(Node node)
{
	//	write("Looking for content in %s\n", node->get_attributes()["Name"]);
	array(Node) all_children = node->get_children();
	foreach(all_children, Node child)
	{
		//		write("Seeing sub tag %s\n", child->get_tag_name());
		if (child->get_tag_name() == "File")
			return 1;
		if (child->get_tag_name() == "Filter" &&
			hasContent(child))
			return TRUE;
	}
	//	write("Saw no content in %s\n", node->get_attributes()["Name"]);
	return FALSE;
}

/**
 * Compare-function for case-insensitive sorting
 */
int icomp(string a, string b)
{
  a = lower_case(a);
  b = lower_case(b);
  if (a == b)
    return 0;
  if (a < b)
    return -1;
  return 1;
}

/**
 * Sorts the children in a node alphabetically and removes empty
 * groups.
 */
int(0..1) sortAndCleanNodeChildren(Node node_to_sort)
{
	if (node_to_sort->get_tag_name() != "Filter")
	{
		write("Will not sort children to anything but filter nodes\n");
		return FALSE;
	}

	//	write("Entering %s\n", node_to_sort->get_attributes()["Name"]);
	mapping (string:Node) folder_children = ([]);
	mapping (string:Node) file_children = ([]);

	array(Node) all_children = node_to_sort->get_children();

	array(Node) new_children = ({});

	for (int i = 0; i < sizeof(all_children); i++)
	{
		Node child = all_children[i];
		if (child->get_tag_name() == "Filter")
		{
			string filter_name = child->get_attributes()["Name"];
			if (!filter_name)
				error("Null filter name?!?\n");
			sortAndCleanNodeChildren(child);
			if (!hasContent(child))
			{
				write ("Removing empty group %s\n", filter_name);
			}
			else
			{
				folder_children[filter_name] = child;
			}
		}
		else if (child->get_tag_name() == "File")
		{
			string file_name = getFileName(child);
			if (sizeof(child->get_children()) == 0)
				error ("Missing text in file node before for %s\n", file_name);
			file_children[file_name] = child;
			/*          if (node_to_sort->get_attributes()["Name"] == "dom")
						write("Adding file %s (%s) to dom\n",
						file_name,
						child->get_attributes()["RelativePath"]); */
			if (sizeof(child->get_children()) == 0)
				error ("Missing text in file node after for %s\n", file_name);
		}
		else // probably text node. Let's skip
		{
			//write("Unidentified node type: %O\n", child);
			//new_children += ({ child });
		}
	}

	array(string) folder_names = Array.sort_array(indices(folder_children), icomp);
	array(string) file_names = Array.sort_array(indices(file_children), icomp);

	/*  if (node_to_sort->get_attributes()["Name"] == "dom")
		write("Files: %O\n", file_names); */

	foreach (file_names, string file_name)
	{
		Node file_node = file_children[file_name];
		new_children += ({ file_node });
		if (sizeof(file_node->get_children()) == 0)
			error ("Missing text in file node");
		/*      string file_path = file_node->get_attributes()["RelativePath"];
				array(string) file_path_arr = file_path/"\\";
				if (file_path_arr[1] == "dom" || file_path_arr[2] == "dom")
				{
				write ("Putting %s in %s\n",
				file_path,
				node_to_sort->get_attributes()["Name"]);
				}
		*/
	}

	foreach (folder_names, string folder_name)
	{
		Node folder_node = folder_children[folder_name];
		new_children += ({ folder_node });
	}

	node_to_sort->replace_children(new_children);

	//  write("Leaving %s\n", node_to_sort->get_attributes()["Name"]);
	return TRUE;
}


/**
 * Checks all files in a module.
 *
 * @param lines the module.sources file.
 */
int syncFiles(Node module_node, string dir, object lines, string top_dir_name, int(0..1) include_top_dir_in_paths, array(string) no_warning_error_files, array(string) compile_for_speed_files)
{
	/*	write("syncFiles (%O,%O,%O, %O, %O)\n",
		  module_node, dir, lines,
		  top_dir_name, include_top_dir_in_paths);
	*/
	int changed = FALSE;
    for (; lines; lines->next())
	{
		string stripped = trim_all_whites(lines->value());

		// Remove comments
		string file_name = extractModuleFile(stripped);
		mapping file_properties = extractModuleFileProperties(stripped);
		//		write("    In module.sources, there were the file '"+
		//                file_name+"'.\n");

		if (strlen(file_name) > 0 &&
			syncFile(module_node, file_name, combine_path(dir, file_name),
					 file_properties,
					 top_dir_name, include_top_dir_in_paths, no_warning_error_files, compile_for_speed_files))
		{
			changed = TRUE;
		}
	} //for

    array all_files = module_node->get_descendants(0);
    for (int i = 0; i < sizeof(all_files); i++)
	{
		// Is the file one that hasn't been seen in any module.source
		// file, then we have to remove it from the build
		// configuration.
		if (isUnmarkedFileNode(all_files[i]))
		{
			Node file_node = all_files[i];
			Node group = file_node->get_parent();
			write("Removing %s from %s\n",
				  file_node->get_attributes()["RelativePath"],
				  getGroupNodePath(group));
			file_node->remove_node();
			// remove text
			if (i+1 < sizeof(all_files) &&
				all_files[i+1]->get_node_type() == XML_TEXT &&
				trim_all_whites(all_files[i+1]->get_text()) == "")
			{
				i++;
				all_files[i]->remove_node();
			}
			else if (i > 0 &&
					 all_files[i-1]->get_node_type() == XML_TEXT &&
					 trim_all_whites(all_files[i-1]->get_text()) == "")
			{
				all_files[i-1]->remove_node();
			}
			changed = TRUE;
		}
		else
		{
			unmarkFileNode(all_files[i]);
		}
	}

    if (sortAndCleanNodeChildren(module_node))
	{
		changed = TRUE;
	}
    // TODO: Remove empty groups

    return changed;
} //syncFiles()


/**
 * Part of the xml renderer. To get small diffs we try to output
 * attributes in the same order as Visual Studio.
 */
void add_hardcoded_attributes(array hardcoded_attributes,
                              mapping attr,
                              function(string:string) attrq,
                              string indentation,
                              Buffer data)
{
	foreach(hardcoded_attributes, string a)
	{
		if (attr[a])
		{
			data->add("\r\n", indentation, a,
					  "=\"", attrq(attr[a]), "\"");
			m_delete(attr, a);
		}
	}
}

/**
 * Part of the XML renderer. We want our output to look the same as
 * the Visual Studio output. This might not be needed in Visual Studio
 * 7.1 (.NET 2003) but it makes diffs smaller at least.
 */
void vcproj_render_xml(String.Buffer data, Node n,
                       function(string:string) textq,
                       function(string:string) attrq,
                       string indentation)
{
	constant tool_hardcoded_attributes = ({
		"Name",
		"AdditionalOptions",
		"AdditionalDependencies",
		"OutputFile",
		"LinkIncremental",
		"Optimization",
		"InlineFunctionExpansion",
		"EnableIntrinsicFunctions",
		"FavorSizeOrSpeed",
		"WholeProgramOptimization",
		"AdditionalIncludeDirectories",
		"PreprocessorDefinitions",
		"MkTypLibCompatible",
		"Culture",
		"StringPooling",
		"MinimalRebuild",
		"ExceptionHandling",
		"BasicRuntimeChecks",
		"RuntimeLibrary",
		"StructMemberAlignment",
		"BufferSecurityCheck",
		"EnableFunctionLevelLinking",
		"TreatWChar_tAsBuiltInType",
		"RuntimeTypeInfo",
		"UsePrecompiledHeader",
		"PrecompiledHeaderThrough",
		"PrecompiledHeaderFile",
		"AssemblerOutput",
		"AssemblerListingLocation",
		"ObjectFile",
		"ProgramDataBaseFileName",
		"BrowseInformation",
		"BrowseInformationFile",
		"WarningLevel",
		"SuppressStartupBanner",
		"AdditionalLibraryDirectories",
		"GenerateManifest",
		"IgnoreDefaultLibraryNames",
		"GenerateDebugInformation",
		"ProgramDatabaseFile",
		"GenerateMapFile",
		"MapFileName",
		"DebugInformationFormat",
		"CompileAs",
		"UndefinePreprocessorDefinitions",
		"SubSystem",
		"StackReserveSize",
		"OptimizeReferences",
		"LinkTimeCodeGeneration",
		"BaseAddress",
		"ImportLibrary",
		"TargetEnvironment",
		"TypeLibraryName",
		"RandomizedBaseAddress",
		"DataExecutionPrevention",
		"TargetMachine"
	});

	constant hardcoded_attributes = ({
		"ProjectType",
		"Version",
		"Name",
		"ProjectGUID",
		"RootNamespace",
		"OutputDirectory",
		"IntermediateDirectory",
		"ConfigurationType",
		"InheritedPropertySheets",
		"UseOfMFC",
		"UseOfATL",
		"ATLMinimizesCRunTimeLibraryUsage",
		"CharacterSet",
		"SccProjectName",
		"SccLocalPath",
		"ManagedExtensions",
		"DeleteExtensionsOnClean",
		"WholeProgramOptimization",
		"BuildLogFile",
		"SuppressStartupBanner",
		"AdditionalOptions",
		"AdditionalDependencies",
		"OutputFile",
		"LinkIncremental",
		"Optimization",
		"InlineFunctionExpansion",
		"PreprocessorDefinitions",
		"Culture",
		"AdditionalIncludeDirectories",
		"StringPooling",
		"BasicRuntimeChecks",
		"RuntimeLibrary",
		"StructMemberAlignment",
		"EnableFunctionLevelLinking",
		"UsePrecompiledHeader",
		"PrecompiledHeaderThrough",
		"PrecompiledHeaderFile",
		"AssemblerOutput",
		"AssemblerListingLocation",
		"ObjectFile",
		"ProgramDataBaseFileName",
		"BrowseInformation",
		"BrowseInformationFile",
		"WarningLevel",
		"SuppressStartupBanner",
		"GenerateDebugInformation",
		"ProgramDatabaseFile",
		"GenerateMapFile",
		"MapFileName",
		"SubSystem",
		"StackReserveSize",
		"OptimizeReferences",
		"DebugInformationFormat",
		"CompileAs",
		"UndefinePreprocessorDefinitions"
	});
    string tagname;
    switch(n->get_node_type()) {
    case XML_TEXT:
		if (trim_all_whites(n->get_text()) != "")
		{
			error("Project file contains text which wouldn't be preserved");
		}
		//      data->add(textq(n->get_text()));
		break;

    case XML_ELEMENT:
		if (!sizeof(tagname = n->get_short_name()))
			break;
		//      write("indentation for %s is '%s'\n", tagname, indentation);
		data->add(indentation, "<", tagname);
		int nof_attrs = 0;
		if (mapping attr = n->get_attributes()) {
			nof_attrs = sizeof(attr);
			if (n->get_tag_name() == "Tool" &&
				attr["Name"] != "VCBscMakeTool" && attr["Name"] != "VCResourceCompilerTool")
			{
				add_hardcoded_attributes(tool_hardcoded_attributes,
										 attr, attrq, indentation+"\t", data);
			}
			else
			{
				add_hardcoded_attributes(hardcoded_attributes,
										 attr, attrq, indentation+"\t", data);
			}

			foreach(indices(attr), string a)
				data->add("\r\n", indentation, "\t", a,
						  "=\"", attrq(attr[a]), "\"");
		}

		if (nof_attrs)
		{
			data->add("\r\n", indentation);
			if (n->count_children())
			{
				data->add("\t");
			}
		}

//		data->add(sprintf(" #sizeof: %d# ", nof_attrs));

		if (n->count_children())
			data->add(">\r\n");
		else
			data->add("/>\r\n");
		break;

    case XML_HEADER:
#if __VERSION__ <7.7
		data->add("<?xml");
		if (mapping attr = n->get_attributes() + ([])) {
			// version and encoding must come in the correct order.
			// version must always be present.
			if (attr->version)
				data->add(" version=\"", attrq(attr->version), "\"");
			else
				data->add(" version='1.0'");
			m_delete(attr, "version");
			if (attr->encoding)
				data->add(" encoding = \"", attrq(attr->encoding), "\"");
			m_delete(attr, "encoding");
			foreach(indices(attr), string a)
				data->add(" ", a, "='", attrq(attr[a]), "'");
		}
		data->add("?>\r\n");
#endif
		break;

    case XML_PI:
		data->add("<?", n->get_short_name());
		string text = n->get_text();
		if (sizeof(text))
			data->add(" ", text);
		data->add("?>");
		break;

    case XML_COMMENT:
		data->add("<!--", n->get_text(), "-->");
		break;
    } //switch

    array(Node) children = n->get_children();
    string new_indentation = indentation;
    if (n->get_node_type() == XML_ELEMENT)
	{
		//      write("In %O - increasing indentation for children\n", n);
		new_indentation = indentation + "\t";
	}

    foreach(children, Node n) {
		vcproj_render_xml(data, n, textq, attrq, new_indentation);
    }

    if (n->get_node_type() == XML_ELEMENT) {
		if (n->count_children())
			if (sizeof(tagname))
				data->add(indentation, "</", tagname, ">\r\n");
    }
}

array(string) fileToArray(string file_name)
{
	array(string) strings = ({});
	if (is_file(file_name))
	{
		FILE file = FILE(file_name, "r");
		object lines = file->line_iterator(1);

	    for (; lines; lines->next())
	    {
			string filename = lines->value();
			int index = search(filename, "#");
			if (index != -1)
				filename = trim_all_whites(filename[..index-1]);
			else
				filename = trim_all_whites(filename);
			if (sizeof(filename) > 0)
			{
				for (int i = 0; i < sizeof(filename); i++)
					if (filename[i] == '/')
						filename[i] = '\\';
				strings += ({filename});
			}
		}
	}
	return strings;
}

/**
 * Determine whether or not pertinent files have changed
 */
int filesHaveChanged(string base_dir, Node xml_node )
{
	//array(string) modules_list = GetModules(base_dir); //old version

	string warnings_file = combine_path(base_dir, "adjunct/desktop_util/quick-warnings.txt");
	array(string) no_warning_error_files = fileToArray(warnings_file);

	string speed_file = combine_path(base_dir, "platforms/windows/optimize_speed.txt");
	array(string) compile_for_speed_files = fileToArray(speed_file);

	write (compile_for_speed_files[0]);

	string modules_dir = combine_path(base_dir, "modules");
	array(string) modules_list = GetModules(modules_dir);
	int module_changes = countChanges("modules", modules_dir, modules_list, xml_node, no_warning_error_files, compile_for_speed_files);

	string adjunct_dir = combine_path(base_dir, "adjunct");
	array(string) adjunct_list = GetModules(adjunct_dir);
	int adjunct_changes = countChanges("adjunct", adjunct_dir, adjunct_list, xml_node, no_warning_error_files, compile_for_speed_files);

	string platforms_dir = combine_path(base_dir, "platforms");
	int windows_changes = countChanges("platforms", platforms_dir, ({"windows", "vega_backends", "media_backends"}), xml_node, no_warning_error_files, compile_for_speed_files);

	return module_changes + adjunct_changes + windows_changes;
}


/**
 * Locates and returns a node for a module. If it doesn't already
 * exist, it's created.
 */
Node getModuleNode(string branch_name, Node root_node, string module_name)
{
	Node vis_node = root_node->get_elements("VisualStudioProject")[0];
	Node files_node = vis_node->get_elements("Files")[0];
	// Locate modules node
	Node source_files = getChildWithNameAttr(files_node, "Source Files");
	//old version with hardcoded "modules" in the call (no good for "adjunct"):
	//Node modules = getChildWithNameAttr(source_files, "modules");
	Node modules = getChildWithNameAttr(source_files, branch_name);
	Node module = getChildWithNameAttrNoError(modules, module_name);

	if (!module)
	{
		mapping attrs = ([ "Name" : module_name ]);
		module = Node(XML_ELEMENT, "Filter", attrs, "");
		write("Adding module %s\n", module_name);
		modules->add_child(module);
	}
	return module;
} //getModuleNode()



/**
 * Return the number of changes found in modules_list compared to
 * the Node-representation of the project file.
 */
int countChanges(string branch_name, string search_dir,
				array(string) modules_list, Node proj_file_node, array(string) no_warning_error_files, array(string) compile_for_speed_files)
{
	int changes = 0;
	foreach (modules_list, string module_name)
	{
		//		write("Looking for module "+module_name+" in "+branch_name+".\n");
		if (isDisabledModule(module_name))
		{
			//			write("...It is excluded\n");
			continue;
		}

		string module_dir = combine_path(search_dir, module_name);
		if (file_stat(module_dir))
		{
			string module_sources = combine_path(module_dir, "module.sources");

			if (is_file(module_sources))
			{
				//			write("Found module.sources\n");
				Node module_node = getModuleNode(branch_name,
												 proj_file_node,
												 module_name);
				FILE file = FILE(module_sources, "r");
				if (syncFiles(module_node, module_dir, file->line_iterator(1),
							  branch_name, TRUE, no_warning_error_files, compile_for_speed_files))
					changes++;
			}
			else
			{
				write("Found no module.sources in module "+module_name+"\n");
			}
		}
	} //foreach

	return changes;
} //countChanges()


/**
 * The main function. The entry point.
 */
int main(int argc, array(string) argv)
{
	string base_dir = dirname(__FILE__);

	Node root_xml_node =
		Parser.XML.Tree.parse_file(combine_path( base_dir, "Opera.vcproj"));

	string co_dir = combine_path(base_dir, "../..");

	if ( filesHaveChanged(co_dir, root_xml_node) )
	{
		/**
		 * the writeNewProjFile-function appears to return non-zero
		 * every now and then even when absolutely nothing in the
		 * environment has changed except for this pike script
		 * itself. However, little or no harm seems to come from this.
		 */
		if (writeNewProjFile(base_dir, "Opera.vcproj", root_xml_node))
		{
			// Need to signal an error for Visual Studio so that the
			// rebuilt vcproj-file can be used.
			write("ALERT: a new version of Opera.vcproj has been created and must be reloaded into VisualStudio \n");
			//write("ALERT: This pike script will therefore perform exit(1), which causes an error message to be displayed\n");
			exit(1);
		}
		else
		{
			write("The file Opera.vcproj was already up to date\n");
		}
	}
	return 0;

}
