/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Anders Oredsson
*/

#include "core/pch.h"

#ifdef SELFTEST
#include "modules/selftest/operaselftest.h"
#include "modules/selftest/optestsuite.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/htmlify.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/domenvironment.h"

OP_STATUS OperaSelftest::GenerateData()
{
	RETURN_IF_ERROR(WriteHead());
	RETURN_IF_ERROR(WriteJavascript());
	RETURN_IF_ERROR(WriteBody());
	RETURN_IF_ERROR(WriteBodyHeaderForm());
	RETURN_IF_ERROR(WriteBodyTables());
	RETURN_IF_ERROR(WriteBodyFooterForm());
	RETURN_IF_ERROR(WriteFinal());
	return OpStatus::OK;
}

OP_STATUS OperaSelftest::WriteHead(){
	// Initialize, open HEAD and write TITLE and stylesheet link

	OpString styleurl;
	TRAP_AND_RETURN(rc, g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleSelftestFile, &styleurl));

	RETURN_IF_ERROR(OpenDocument(UNI_L("opera:selftest"), styleurl.CStr()));

	//Dont know why, but this is needed!
	OpString line;
	line.Reserve(20);
	RETURN_IF_ERROR(line.Set("</link>\n"));
	return m_url.WriteDocumentData(URL::KNormal, line);
}
OP_STATUS OperaSelftest::WriteJavascript(){
	OpFile selftest_js;
	RETURN_IF_ERROR(selftest_js.Construct(UNI_L("selftest.js"), OPFILE_SCRIPT_FOLDER));

	const uni_char *script_template = UNI_L("<script type=\"text/javascript\" src=\"%s\"></script>\n");

	OpString selftest_js_url;

	TRAPD(rc, g_url_api->ResolveUrlNameL(selftest_js.GetFullPath(), selftest_js_url););
	RETURN_IF_ERROR(rc);

	uni_char *htmlified_selftest = HTMLify_string(selftest_js_url.CStr());
	if (!htmlified_selftest)
		return OpStatus::ERR_NO_MEMORY;
	
	m_url.WriteDocumentDataUniSprintf(script_template, htmlified_selftest);

	OP_DELETEA(htmlified_selftest);
	return OpStatus::OK;
}
OP_STATUS OperaSelftest::WriteBody(){
	OP_STATUS rc;
	OpString line;
	line.Reserve(1000);
	rc = line.Set(
		"</head>\n"
		"\n"
		"<body onload=\"OnLoad()\" onresize=\"SetSize()\">\n"
		"<div id=\"wrap\">\n"
	);
	RETURN_IF_ERROR(rc);
	return m_url.WriteDocumentData(URL::KNormal, line);
}
OP_STATUS OperaSelftest::WriteBodyHeaderForm(){
	OpString line;
	line.Reserve(5000);
	RETURN_IF_ERROR(line.Set(
		"<div id=\"header\">\n"
		"	<h1 style=\"margin-top: 0\">Selftest</h1>\n"
		"	<div id=\"toolbar\">\n"
		"		<form name=\"f\">\n"
		"			<input name=\"a\" type=\"text\" value=\"-test\" />\n"
		"			<input name=\"r\" type=\"submit\" value=\"Run\" onclick=\"OnRun(); return false;\" />\n"
		"			<input type=\"submit\" value=\"Clear output\" onclick=\"OnClearOutput(); return false;\" />\n"
		"			<label><input name=\"s\" type=\"checkbox\" onclick=\"DoScroll();\" checked />Scroll automatically (slows things down a bit)</label>\n"
		"			<br />\n"
		"			Output to file (optional):<input name=\"outputfile\" type=\"text\" />\n"
		"			&nbsp&nbsp<label><input name=\"runmanual\" type=\"checkbox\" />Run manual tests&nbsp&nbsp	</label>\n"	
		"			<!-- <input name=\"machine\" type=\"checkbox\" />Spartan Readable -->\n"
		"			<br />\n"
	));
	
	//Find OPFILE_SELFTEST_DATA_FOLDER
	RETURN_IF_ERROR(line.Append("			<input name=\"testdata\" type=\"text\" size=\"45\" disabled value=\""));
	OpString folder_path_name;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_SELFTEST_DATA_FOLDER, folder_path_name));
	RETURN_IF_ERROR(line.Append(folder_path_name));
	RETURN_IF_ERROR(line.Append("\" />\n"));

	RETURN_IF_ERROR(line.Append(
		"			<label><input name=\"override\" type=\"checkbox\" onchange=\"OnOverridePathChangedEvent(this.checked);\" unchecked />Override default testdata directory</label>\n"
		"		</form>\n"
		"	</div>\n"
		"</div>\n"
	));
	return m_url.WriteDocumentData(URL::KNormal, line);
}
OP_STATUS OperaSelftest::WriteBodyTables(){
	OP_STATUS rc;
	OpString line;
	line.Reserve(5000);
	rc = line.Set(
		"<div id=\"outputcontainer\">\n"
		"		<pre id=\"output\"><span>&nbsp;</span></pre>\n"
		"</div>\n"
		"<div id=\"testselector\">\n"
		"		<strong> -- Modules -- </strong><br />\n"
		"		<label><input id=\"runalltests\" type=\"checkbox\" onchange=\"OnSelectModuleChangedEvent(this.checked)\"/><i>All modules</i></label><br />\n"
	);
	RETURN_IF_ERROR(rc);
	int i = 0;

	//find all modules, and add them as chackboxes!
	const char *name, *module_name;
	const char *previous_module_name = NULL;
	while(TestSuite::GroupInfo(i, name, module_name))
	{
		if(previous_module_name == NULL || op_strcmp(module_name, previous_module_name))
		{
			uni_char* module_name_uni = uni_up_strdup(module_name);
			if (module_name_uni)
			{
				RETURN_IF_ERROR(line.Append("<label><input id=\""));
				RETURN_IF_ERROR(line.Append(module_name_uni));
				RETURN_IF_ERROR(line.Append("\" type=\"checkbox\" onchange=\"OnSelectModuleChangedEvent(this.checked)\"/>\n"));
				RETURN_IF_ERROR(line.Append(module_name_uni));	
				RETURN_IF_ERROR(line.Append("</label><br />\n"));
				op_free(module_name_uni);
			}
		}
		previous_module_name = module_name;
		i++;
	}
	rc = line.Append(
		"</div>\n"
	);
	RETURN_IF_ERROR(rc);
	return m_url.WriteDocumentData(URL::KNormal, line);
}

OP_STATUS OperaSelftest::WriteBodyFooterForm(){
	OpString line;
	line.Reserve(5000);
	RETURN_IF_ERROR(line.Set(
		"<div id=\"footer\">\n"
		"	<form name=\"publishToServer\">\n"
		"			POST to server: "
		"			Host(http://etc..): <input name=\"publishToHost\" type=\"text\" value=\"empty\" />\n"
		"			FieldName: <input name=\"publishOutputAsField\" type=\"text\" value=\"empty\" />\n"
		"			<input name=\"submit\" type=\"submit\" value=\"POST to Host\" onclick=\"OnPublish(); return false;\" />\n"
		"	</form>\n"
		"</div>\n"
	));
	return m_url.WriteDocumentData(URL::KNormal, line);
}
OP_STATUS OperaSelftest::WriteFinal(){
	OP_STATUS rc;
	OpString line;
	line.Reserve(50);
	rc = line.Set(
		"</div>\n"	//wrap
	);
	RETURN_IF_ERROR(rc);
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
	return CloseDocument();//head and body!
}

#endif
