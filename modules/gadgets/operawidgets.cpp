/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined OPERAWIDGETS_URL || defined OPERAEXTENSIONS_URL

#include "modules/gadgets/operawidgets.h"
#include "modules/locale/locale-enum.h"

OP_STATUS OperaWidgets::GenerateData()
{
#ifdef OPERAEXTENSIONS_URL
	if (Widgets == m_pagetype)
	{
#endif
#ifdef _LOCALHOST_SUPPORT_
		RETURN_IF_ERROR(OpenDocument(Str::S_WIDGETS_TITLE, PrefsCollectionFiles::StyleWidgetsFile));
#else
		RETURN_IF_ERROR(OpenDocument(Str::S_WIDGETS_TITLE));
#endif
#ifdef OPERAEXTENSIONS_URL
	}
	else
	{
# ifdef _LOCALHOST_SUPPORT_
		RETURN_IF_ERROR(OpenDocument(Str::S_EXTENSIONS_TITLE, PrefsCollectionFiles::StyleWidgetsFile));
# else
		RETURN_IF_ERROR(OpenDocument(Str::S_EXTENSIONS_TITLE));
# endif
	}
#endif

	RETURN_IF_ERROR(OpenBody());

	OpString line;

	OpString open_widget, close_widget, delete_widget, widget_properties, widget_options;
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	OpString widget_check_update;
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
#ifdef OPERAEXTENSIONS_URL
	if (Widgets == m_pagetype)
	{
#endif
		RETURN_IF_ERROR(JSSafeLocaleString(&open_widget, Str::S_WGTMGR_CMD_OPEN_WIDGET));
		RETURN_IF_ERROR(JSSafeLocaleString(&close_widget, Str::S_WM_CLOSE_WIDGET));
#ifdef OPERAEXTENSIONS_URL
	}
	else
	{
		RETURN_IF_ERROR(JSSafeLocaleString(&open_widget, Str::S_WEBSERVER_START_SERVICE));
		RETURN_IF_ERROR(JSSafeLocaleString(&close_widget, Str::S_WEBSERVER_STOP_SERVICE));
		RETURN_IF_ERROR(JSSafeLocaleString(&widget_options, Str::S_WGTMGR_TITLE_OPTIONS));
	}
#endif
	RETURN_IF_ERROR(JSSafeLocaleString(&delete_widget, Str::S_WGTMGR_CMD_DELETE_WIDGET));
	RETURN_IF_ERROR(JSSafeLocaleString(&widget_properties, Str::D_WIDGET_PROPERTIES));
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(JSSafeLocaleString(&widget_check_update, Str::S_WIDGET_UPDATE_BUTTON));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON

	RETURN_IF_ERROR(line.Append(
		"\n<script type='text/javascript'>\n"
		"	window.addEventListener('load', init, false);\n\n"
		"	function init()"
		"	{\n"));
#ifdef OPERAEXTENSIONS_URL
	if (Widgets == m_pagetype)
	{
#endif
		RETURN_IF_ERROR(line.Append(
		"		if(opera && opera.widgetManager)"
		"		{\n"
		"			var wmgr = opera.widgetManager;\n"));
#ifdef OPERAEXTENSIONS_URL
	}
	else
	{
		OP_ASSERT(Extensions == m_pagetype);
		RETURN_IF_ERROR(line.Append(
		"		if(opera && opera.extensionManager)"
		"		{\n"
		"			var wmgr = opera.extensionManager;\n"));
	}
#endif
	RETURN_IF_ERROR(line.Append(
		"			var wdgts = wmgr.widgets;\n"
		"\n"
		"			var list = document.getElementById('gadget_list');\n"
		"			var olay = document.getElementById('overlay');\n"
		"			var info = document.getElementById('info');\n"
		"			var btn_open = document.getElementById('btn_open');\n"
		"			var btn_close = document.getElementById('btn_close');\n"
		"			var btn_del = document.getElementById('btn_del');\n"
		"			var btn_info = document.getElementById('btn_info');\n"));
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
		RETURN_IF_ERROR(line.Append("			var btn_update = document.getElementById('btn_update');\n"));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
		RETURN_IF_ERROR(line.Append("			var btn_update_url = document.getElementById('btn_update_url');\n"));
#ifdef OPERAEXTENSIONS_URL
	if (Extensions == m_pagetype)
		RETURN_IF_ERROR(line.Append(
				"			var btn_options = document.getElementById('btn_options');\n"));
#endif
	RETURN_IF_ERROR(line.Append(
		"			var sel_wmode = document.getElementById('sel_wmode');\n"
		"\n"
		"			btn_open.addEventListener('click', hndl_click, false);\n"
		"			btn_close.addEventListener('click', hndl_click, false);\n"
		"			btn_del.addEventListener('click', hndl_click, false);\n"
		"			btn_info.addEventListener('click', hndl_click, false);\n"
		"			list.addEventListener('keyup', hndl_keyup, false);\n"));
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append(
		"			btn_update.addEventListener('click', hndl_click, false);\n"));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append(
		"			btn_update_url.addEventListener('click', hndl_click, false);\n"));
#ifdef OPERAEXTENSIONS_URL
	if (Extensions == m_pagetype)
		RETURN_IF_ERROR(line.Append(
		"			var btn_https = document.getElementById('btn_https');\n"
		"			var btn_private = document.getElementById('btn_private');\n"
		"			btn_https.addEventListener('click', hndl_https, false);\n"
		"			btn_private.addEventListener('click', hndl_private, false);\n"
		"			list.addEventListener('change', hndl_list_select, false);\n"));
#endif
#ifdef OPERAEXTENSIONS_URL
	if (Extensions == m_pagetype)
		RETURN_IF_ERROR(line.Append(
				"			btn_options.addEventListener('click', hndl_click, false);\n"));
#endif
	if (Widgets == m_pagetype)
		RETURN_IF_ERROR(line.Append(
		"			sel_wmode.addEventListener('change', set_wmode, false);\n"));

	RETURN_IF_ERROR(line.Append(
		"\n"
		"			list_gadgets();\n"
		"		}\n"
		"		function list_gadgets()"
		"		{\n"
		"			list.innerHTML = '';\n"
		"			list.style.height  = '' + parseInt(window.innerHeight * 0.6) + 'px';\n"
		"			for (var i = 0; wdgts[i]; ++i)"
		"			{\n"
		"				var item = document.createElement('option');\n"
		"				witem = wdgts[i];\n"
		"				item.id = 'wdgt_' + i;\n"
		"				item.value = i;\n"));
#ifdef OPERAEXTENSIONS_URL
	if (Extensions == m_pagetype)
		RETURN_IF_ERROR(line.Append(
		"				item.allow_https = true;\n"
		"				item.allow_private = false;\n"));
#endif
	RETURN_IF_ERROR(line.Append(
		"				item.appendChild(document.createTextNode(witem.name));\n"
		"				list.appendChild(item);\n"
		"			}\n"
		"		}\n"
		"\n"));
#ifdef OPERAEXTENSIONS_URL
	if (Extensions == m_pagetype)
		RETURN_IF_ERROR(line.Append(
		"		function colourCode(priv_enabled, https_enabled) {\n"
		"			var colour = priv_enabled ? 'green' : (!https_enabled ? 'lightgreen' : 'white');\n"
		"			return colour;\n"
		"		}\n"
		"		function hndl_https(evt){\n"
		"			var list = document.getElementById('gadget_list');\n"
		"			if (list.selectedIndex >= 0) {\n"
		"			    var elt = list.options[list.selectedIndex];\n"
		"			    elt.allow_https = !elt.allow_https;\n"
		"			    wdgts[list.selectedIndex].setExtensionProperty('allow_https', elt.allow_https);\n"
		"			    elt.style.backgroundColor = colourCode(elt.allow_private, elt.allow_https);\n"
		"			}\n"
		"		}\n"
		"		function hndl_private(evt){\n"
		"			var list = document.getElementById('gadget_list');\n"
		"			if (list.selectedIndex >= 0) {\n"
		"			    var elt = list.options[list.selectedIndex];\n"
		"			    elt.allow_private = !elt.allow_private;\n"
		"			    wdgts[list.selectedIndex].setExtensionProperty('allow_private', elt.allow_private);\n"
		"			    elt.style.backgroundColor = colourCode(elt.allow_private, elt.allow_https);\n"
		"			}\n"
		"		}\n"
		"		function hndl_list_select(evt){\n"
		"			if (evt.target && evt.target.selectedIndex >= 0) {\n"
		"			    var el_id = evt.target.selectedIndex;\n"
		"			    var is_https = evt.target.options[el_id].allow_https;\n"
		"			    var is_priv = evt.target.options[el_id].allow_private;\n"
		"			    document.getElementById('btn_https').checked = is_https;\n"
		"			    document.getElementById('btn_private').checked = is_priv;\n"
		"		    }\n"
		"		}\n"));
#endif
	RETURN_IF_ERROR(line.Append(
		"		function hndl_click(evt)"
		"		{\n"
		"			if (evt.target && evt.target.nodeType == 1)"
		"			{\n"
		"				var el_id = evt.target.id;\n"
		"				var ar_act = new Array();\n"
		"				for (var i = 0; list.options[i]; ++i)"
		"				{\n"
		"					var opt = list.options[i];\n"
		"					if (opt.selected)"
		"					{\n"
		"						ar_act.push(opt.value);\n"
		"					}\n"
		"				}\n"
		"				var action;\n"
		"				var extra_args = '';\n"
		"				switch (el_id)"
		"				{\n"
		"					case 'btn_open':\n"
		"						action = 'run';\n"));
#ifdef OPERAEXTENSIONS_URL
	if (Extensions == m_pagetype)
		RETURN_IF_ERROR(line.Append(
		"						extra_args = list.selectedIndex < 0 ? '' : (', ' + list.options[list.selectedIndex].allow_https + ', ' + list.options[list.selectedIndex].allow_private);\n"));
#endif
	RETURN_IF_ERROR(line.Append(
		"						break;\n"
		"					case 'btn_close':\n"
		"						action = 'kill';\n"
		"						break;\n"
		"					case 'btn_del':\n"
		"						action = 'uninstall';\n"
		"						break;\n"
		"					case 'btn_info':\n"
		"						action = 'info';\n"
		"						break;\n"));
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append(
		"					case 'btn_update':\n"
		"						action = 'checkForUpdate';\n"
		"						break;\n"));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append(
		"					case 'btn_update_url':\n"
		"						//action = 'checkForUpdateByUrl';\n"
		"						//extra_args = ', '+document.getElementById('update_url').value\n"
		"						wmgr.checkForUpdateByUrl(document.getElementById('update_url').value)\n"
		"						break;\n"));
	RETURN_IF_ERROR(line.Append(
		"					case 'btn_options':\n"
		"						action = 'options';\n"
		"						break;\n"
		"				}\n"
		"				if (action != 'info')"
		"				{\n"
		"					if (action == 'options' && !wmgr.options) return;\n"
		"					for (var i=ar_act.length - 1; ar_act[i]; --i){\n"
		"						eval ('wmgr.' + action + '(wdgts[' + ar_act[i] + ']' + extra_args + ')');\n"
		"					}\n"
		"					if (action == 'uninstall')"
		"					{\n"
		"						list_gadgets();\n"
		"						list.selectedIndex = !ar_act[0] ? 0 : (ar_act[0] - 1);\n"
		"					}\n"
		"				}"
		"				else"
		"				{\n"
		"					// show info box for a single widget only\n"
		"					show_gadget_info(ar_act[0]);\n"
		"				}\n"
		"			}\n"
		"		}\n"
		"\n"
		"		function hndl_keyup(e)\n"
		"		{\n"
		"			if (e.keyCode == 46)\n"
		"			{\n"
		"				btn_del.click();\n"
		"			}\n"
		"			else if (e.keyCode == 13)\n"
		"			{\n"
		"				btn_open.click();\n"
		"			}\n"
		"		}\n"
		"\n"
		"		function set_wmode(e)\n"
		"		{\n"
		"			var wmodes = e.target.value;\n"
		"			var ar_act = new Array();\n"
		"			for (var i = 0; list.options[i]; ++i){\n"
		"				var opt = list.options[i];\n"
		"				if (opt.selected){\n"
		"					ar_act.push(opt.value);\n"
		"				}\n"
		"			}\n"
		"			for (var i=ar_act.length - 1; ar_act[i]; --i){\n"
		"				while (wmodes.length > 0){\n"
		"					var space_index = wmodes.indexOf(' ');\n"
		"					space_index = space_index > 0 ? space_index : wmodes.length;\n"
		"					var wmode = wmodes.substr(0, space_index);\n"
		"					wmgr.setWidgetMode(wdgts[ar_act[i]], wmode);\n"
		"					wmodes = wmodes.substr(space_index+1);\n"
		"				}\n"
		"			}\n"
		"		}\n"
		"\n"
		"		function show_gadget_info(idx){\n"
		"			var wdgt = wdgts[idx];\n"
		"			if (!wdgt) return;\n"
		"			info.innerHTML = '';\n"
		"			var btnc = document.createElement('button');\n"
		"			btnc.textContent = 'x';\n"
		"			btnc.addEventListener('click', function () {olay.style.display = 'none';}, false);\n"
		"			document.getElementById('dtitle').innerHTML = '' ;\n"
		"			document.getElementById('dtitle').appendChild(btnc);\n"
		"			var tbl = document.createElement('table');\n"
		"			var trow = tbl.appendChild(document.createElement('tr'));\n"
		"			for (x in wdgt){\n"
		"				var item = document.createElement('tr');\n"
		"				item.appendChild(document.createElement('th')).appendChild(document.createTextNode(x));\n"
		"				item.appendChild(document.createElement('td')).appendChild(document.createTextNode(wdgt[x]));\n"
		"				tbl.appendChild(item);\n"
		"			}\n"
		"			var icon_item = document.createElement('tr');\n"
		"			icon_item.appendChild(document.createElement('th')).appendChild(document.createTextNode('icon'));\n"
		"			var icon_img = document.createElement('img');\n"
		"			icon_img.src = wmgr.getWidgetIconURL(wdgt);\n"
		"			icon_item.appendChild(document.createElement('td')).appendChild(icon_img);\n"
		"			tbl.appendChild(icon_item);\n"
		"			info.appendChild(tbl);\n"
		"			olay.style.display = 'block';\n"
		"		}\n"
		"	}\n"
		"</script>\n"
		"<div id='header'>\n"
		"	<h1>"));
#ifdef OPERAEXTENSIONS_URL
	if (Widgets == m_pagetype)
	{
#endif
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MANAGE_WIDGETS));
		RETURN_IF_ERROR(line.Append("	</h1>\n"
		"      <p style='text-align:right'><a href='http://widgets.opera.com/'>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_ADD_WIDGETS));
		RETURN_IF_ERROR(line.Append("</a></p>\n"));
#ifdef OPERAEXTENSIONS_URL
	}
	else
	{
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MANAGE_EXTENSIONS));
		RETURN_IF_ERROR(line.Append("	</h1>\n"		"      <p style='text-align:right'><a href='https://addons.labs.opera.com/addons/extensions/'>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_ADD_EXTENSIONS));
		RETURN_IF_ERROR(line.Append("</a></p>\n"));
	}
#endif
	RETURN_IF_ERROR(line.Append("</h1>"
		"</div>\n"
		"<div id='content'>\n"
		"		<div id='btn_wrapper'>\n"
		"			<button id='btn_open'>"));
	RETURN_IF_ERROR(line.Append(open_widget));
	RETURN_IF_ERROR(line.Append("</button>\n"
		"			<button id='btn_close'>"));
	RETURN_IF_ERROR(line.Append(close_widget));
	RETURN_IF_ERROR(line.Append("</button>\n"
		"			<button id='btn_del'>"));
	RETURN_IF_ERROR(line.Append(delete_widget));
	RETURN_IF_ERROR(line.Append("</button>\n"
		"			<button id='btn_info'>"));
	RETURN_IF_ERROR(line.Append(widget_properties));

#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append("</button>\n"
		"			<button id='btn_update'>"));
	RETURN_IF_ERROR(line.Append(widget_check_update));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append("</button>\n"));

#ifdef OPERAEXTENSIONS_URL
	if (Extensions == m_pagetype)
	{
		RETURN_IF_ERROR(line.Append(
		"			<button id='btn_options'>"));
		RETURN_IF_ERROR(line.Append(widget_options));
		RETURN_IF_ERROR(line.Append("</button>\n"));
		RETURN_IF_ERROR(line.Append(
		"			<input id='btn_https' type='checkbox'>https</input>"
		"			<input id='btn_private' type='checkbox'>private</input>"));
	}
#endif
	if (Widgets == m_pagetype)
	{

		RETURN_IF_ERROR(line.Append("		<label>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WIDGET_VIEW_MODE));
		RETURN_IF_ERROR(line.Append(":\n"
			"		<select id='sel_wmode'>\n"
			"			<option value='floating widget'>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WIDGET_VIEW_MODE_FLOATING));
		RETURN_IF_ERROR(line.Append(
			"</option>\n"
			"			<option value='windowed application'>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WIDGET_VIEW_MODE_WINDOWED));
		RETURN_IF_ERROR(line.Append(
			"</option>\n"
			"			<option value='fullscreen'>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WIDGET_VIEW_MODE_FULLSCREEN));
		RETURN_IF_ERROR(line.Append(
			"</option>\n"
			"			<option value='maximized'>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WIDGET_VIEW_MODE_MAXIMIZED));
		RETURN_IF_ERROR(line.Append(
			"</option>\n"
			"			<option value='minimized docked'>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WIDGET_VIEW_MODE_MINIMIZED));
		RETURN_IF_ERROR(line.Append(
			"</option>\n"
			"		</select>\n"
			"		</label>\n"));
	}
	RETURN_IF_ERROR(line.Append("		</div>\n"));

	RETURN_IF_ERROR(line.Append("		<div id='btn_wrapper'>\n"));
	RETURN_IF_ERROR(line.Append("			<input id='update_url' type='text'"));
	RETURN_IF_ERROR(line.Append(" value='Enter update description document URL here...' style=\"width:60%\"></input>\n"));
	RETURN_IF_ERROR(line.Append("			<button id='btn_update_url'>"));
	RETURN_IF_ERROR(line.Append(widget_check_update));
	RETURN_IF_ERROR(line.Append("			</button>\n"));
	RETURN_IF_ERROR(line.Append("		</div>\n"));

	RETURN_IF_ERROR(line.Append("		<select multiple='multiple' id='gadget_list'><option>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::D_GENERIC_ERROR));
	RETURN_IF_ERROR(line.Append("</option></select>\n"
		"</div>\n"
		"<div id='overlay'><div id='dwrapper'><div id='dtitle'></div><div id='info'></div></div></div>\n"
	));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	RETURN_IF_ERROR(CloseDocument());

	return OpStatus::OK;
}

#endif // OPERAWIDGETS_URL || OPERAEXTENSIONS_URL

