/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef OPERAUNITE_URL

#include "modules/gadgets/operaunite.h"
#include "modules/locale/locale-enum.h"

OP_STATUS
OperaUnite::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_UNITE_TITLE, PrefsCollectionFiles::StyleWidgetsFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_UNITE_TITLE));
#endif

	RETURN_IF_ERROR(OpenBody());

	OpString line;

	OpString start_app, stop_app, open_app, delete_widget, widget_properties,
	         logged_out, creating_account, logging_in, success,
	         log_in, create_account, log_out, cancel;
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	OpString update_widget;
	RETURN_IF_ERROR(JSSafeLocaleString(&update_widget, Str::S_WIDGET_UPDATE_BUTTON));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(JSSafeLocaleString(&start_app, Str::S_WEBSERVER_START_SERVICE));
	RETURN_IF_ERROR(JSSafeLocaleString(&stop_app, Str::S_WEBSERVER_STOP_SERVICE));
	RETURN_IF_ERROR(JSSafeLocaleString(&open_app, Str::MI_IDM_MENU_GOTO_PUBLIC_SERVICE_PAGE));
	RETURN_IF_ERROR(JSSafeLocaleString(&delete_widget, Str::S_WGTMGR_CMD_DELETE_WIDGET));
	RETURN_IF_ERROR(JSSafeLocaleString(&widget_properties, Str::D_WIDGET_PROPERTIES));
	RETURN_IF_ERROR(JSSafeLocaleString(&logged_out, Str::D_FEATURE_SETTINGS_LOGGED_OUT));
	RETURN_IF_ERROR(JSSafeLocaleString(&creating_account, Str::D_FEATURE_SETUP_CREATING_ACCOUNT));
	RETURN_IF_ERROR(JSSafeLocaleString(&logging_in, Str::D_FEATURE_SETUP_LOGGING_IN));
	RETURN_IF_ERROR(JSSafeLocaleString(&success, Str::S_MINI_SYNC_SUCCESS));
	RETURN_IF_ERROR(JSSafeLocaleString(&log_in, Str::D_FEATURE_SETUP_LOGIN));
	RETURN_IF_ERROR(JSSafeLocaleString(&create_account, Str::D_FEATURE_SETUP_CREATE_ACCOUNT));
	RETURN_IF_ERROR(JSSafeLocaleString(&log_out, Str::D_FEATURE_SETTINGS_LOGOUT));
	RETURN_IF_ERROR(JSSafeLocaleString(&cancel, Str::DI_IDCANCEL));

	RETURN_IF_ERROR(line.Append(
		"\n"
		"<script type='text/javascript'>\n"
		"	window.addEventListener('load', init, false);\n"
		"	function init(){\n"
		"		if(opera && opera.uniteApplicationManager){\n"
		"			var wmgr = opera.uniteApplicationManager;\n"
		"			var wdgts = wmgr.widgets;\n"
		"\n"
		"			var list = document.getElementById('gadget_list');\n"
		"			var olay = document.getElementById('overlay');\n"
		"			var info = document.getElementById('info');\n"
		"			var btn_open = document.getElementById('btn_open');\n"
		"			var btn_close = document.getElementById('btn_close');\n"
		"			var btn_go = document.getElementById('btn_go');\n"
		"			var btn_del = document.getElementById('btn_del');\n"
		"			var btn_info = document.getElementById('btn_info');\n"));
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append(
		"			var btn_update = document.getElementById('btn_update');\n"));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append(
		"\n"
		"			btn_open.addEventListener('click', hndl_click, false);\n"
		"			btn_close.addEventListener('click', hndl_click, false);\n"
		"			btn_go.addEventListener('click', hndl_click, false);\n"
		"			btn_del.addEventListener('click', hndl_click, false);\n"
		"			btn_info.addEventListener('click', hndl_click, false);\n"));
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append(
		"			btn_update.addEventListener('click', hndl_click, false);\n"));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append(
		"			list.addEventListener('keyup', hndl_keyup, false);\n"
		"\n"
		"			list_gadgets();\n"
		"		}\n"
		"		function list_gadgets(){\n"
		"			list.innerHTML = ''; //remove the option elements already there.\n"
		"			list.style.height  = '' + parseInt(window.innerHeight * 0.3) + 'px';\n"
		"			for (var i = 0; wdgts[i]; ++i){\n"
		"				var item = document.createElement('option');\n"
		"				witem = wdgts[i];\n"
		"				item.id = 'wdgt_' + i;\n"
		"				item.value = i;\n"
		"				item.appendChild(document.createTextNode(witem.name));\n"
		"				list.appendChild(item);\n"
		"			}\n"
		"		}\n"
		"\n"
		"		function hndl_click(evt){\n"
		"			if (evt.target && evt.target.nodeType == 1){\n"
		"				var el_id = evt.target.id;\n"
		"				var ar_act = new Array();\n"
		"				for (var i = 0; list.options[i]; ++i){\n"
		"					var opt = list.options[i];\n"
		"					if (opt.selected){\n"
		"						ar_act.push(opt.value);\n"
		"					}\n"
		"				}\n"
		"				var action;\n"
		"				switch (el_id){\n"
		"					case 'btn_open':\n"
		"						action = 'run';\n"
		"						break;\n"
		"					case 'btn_close':\n"
		"						action = 'kill';\n"
		"						break;\n"
		"					case 'btn_go':\n"
		"						action = 'open';\n"
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
		"				}\n"
		"				if (action != 'info' && action != 'open'){\n"
		"					for (var i=ar_act.length - 1; ar_act[i]; --i){\n"
		"						eval ('wmgr.' + action + '(wdgts[' + ar_act[i] + '])');\n"
		"					}\n"
		"					if (action == 'uninstall'){\n"
		"						list_gadgets();\n"
		"						list.selectedIndex = !ar_act[0] ? 0 : (ar_act[0] - 1);\n"
		"					}\n"
		"				}else if (action == 'info'){\n"
		"					// show info box for a single widget only\n"
		"					show_gadget_info(ar_act[0]);\n"
		"				}else if (action == 'open'){\n"
		"					for (var i=ar_act.length - 1; ar_act[i]; --i){\n"
		"						if (opera.uniteDeviceManager.range == 3)"
		"							window.open('http://' + opera.uniteDeviceManager.hostName + '/' + wdgts[ar_act[i]].servicePath + '/');\n"
		"						else"
		"							window.open('http://' + opera.uniteDeviceManager.hostName + ':' + opera.uniteDeviceManager.port + '/' + wdgts[ar_act[i]].servicePath + '/');\n"
		"					}\n"
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
		"				// open the widget\n"
		"				// TODO: also don't submit the form\n"
		"				btn_open.click();\n"
		"			}\n"
		"		}\n"
		"\n"
		"		function show_gadget_info(idx){\n"
		"			var wdgt = wdgts[idx];\n"
		"			if (!wdgt) return;\n"
		"			info.innerHTML = '';\n"
		"			var btnc = document.createElement('input');\n"
		"			btnc.type = 'button';\n"
		"			btnc.value = 'x';\n"
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
		"			info.appendChild(tbl);\n"
		"			olay.style.display = 'block';\n"
		"		}\n"
		"	}\n"
		"</script>\n"
		"<div id='header'><h1>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::M_WEBSERVER_MANAGE_SERVICES));
	RETURN_IF_ERROR(line.Append("</h1></div>\n"
		"<div id='content'>\n"
		"	<form>\n"
		"		<div id='btn_wrapper'>\n"
		"			<input id='btn_open' value='"));
	RETURN_IF_ERROR(line.Append(start_app));
	RETURN_IF_ERROR(line.Append("' type='button'>\n"
		"			<input id='btn_close' value='"));
	RETURN_IF_ERROR(line.Append(stop_app));
	RETURN_IF_ERROR(line.Append("' type='button'>\n"
		"			<input id='btn_go' value='"));
	RETURN_IF_ERROR(line.Append(open_app));
	RETURN_IF_ERROR(line.Append("' type='button'>\n"
		"			<input id='btn_del' value='"));
	RETURN_IF_ERROR(line.Append(delete_widget));
#ifdef GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append("' type='button'>\n"
		"			<input id='btn_update' value='"));
	RETURN_IF_ERROR(line.Append(update_widget));
#endif // GADGETS_OPERAWIDGETS_UPDATE_BUTTON
	RETURN_IF_ERROR(line.Append("' type='button'>\n"
		"			<input id='btn_info' value='"));
	RETURN_IF_ERROR(line.Append(widget_properties));
	RETURN_IF_ERROR(line.Append("' type='button'>\n"
		"		</div>\n"
		"		<fieldset>\n"
		"			<legend>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::M_VIEW_HOTLIST_MENU_UNITE_SERVICES));
	RETURN_IF_ERROR(line.Append("</legend>\n"
		"			<select multiple='multiple' id='gadget_list'>\n"
		"				<option>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::D_GENERIC_ERROR));
	RETURN_IF_ERROR(line.Append("</option>\n"
		"			</select>\n"
		"		</fieldset>\n"
		"	</form>\n"
		"</div>\n"
		"<h2>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MANAGE_ACCOUNTS));
	RETURN_IF_ERROR(line.Append("</h2>\n"
		"<div id='content'>\n"
		"	<script type='text/javascript'>\n"
		"		window.addEventListener('load', initialize, false);\n"
		"		var account = opera.operaAccountManager;\n"
		"		var currentOp = '';\n"
		"		function initialize() {\n"
		"			account.onauthenticationchange = onAuthenticationChange;\n"
		"			onAuthenticationChange();\n"
		"		}\n"
		"		function onAuthenticationChange() {\n"
		"			document.login.username.value = account.username;\n"
		"			document.login.savePassword.checked = account.rememberMe;\n"
		"			if (account.authState == 0)\n"
		"			{\n"
		"				document.login.username.disabled = false;\n"
		"				document.login.password.disabled = false;\n"
		"				document.login.email.disabled = false;\n"
		"				document.login.savePassword.disabled = false;\n"
		"				document.login.login.disabled = false;\n"
		"				document.login.create.disabled = false;\n"
		"				document.login.logout.disabled = true;\n"
		"				document.login.abort.disabled = true;\n"
		"				if (currentOp != 'Login' && currentOp != 'Create account')\n"
		"					document.login.password.value = '';\n"
		"				document.getElementById('status').innerHTML = '["));
	RETURN_IF_ERROR(line.Append(logged_out));
	RETURN_IF_ERROR(line.Append("]';\n"
		"			}\n"
		"			else if (account.authState == 1)\n"
		"			{\n"
		"				document.login.username.disabled = true;\n"
		"				document.login.password.disabled = true;\n"
		"				document.login.email.disabled = true;\n"
		"				document.login.savePassword.disabled = true;\n"
		"				document.login.login.disabled = true;\n"
		"				document.login.create.disabled = true;\n"
		"				document.login.logout.disabled = true;\n"
		"				document.login.abort.disabled = false;\n"
		"				if (currentOp == 'Create account')\n"
		"					document.getElementById('status').innerHTML = '["));
	RETURN_IF_ERROR(line.Append(creating_account));
	RETURN_IF_ERROR(line.Append("]';\n"
		"				else\n"
		"					document.getElementById('status').innerHTML = '["));
	RETURN_IF_ERROR(line.Append(logging_in));
	RETURN_IF_ERROR(line.Append("]';\n"
		"			}\n"
		"			else if (account.authState == 2)\n"
		"			{\n"
		"				document.login.username.disabled = true;\n"
		"				document.login.password.disabled = true;\n"
		"				document.login.email.disabled = true;\n"
		"				document.login.savePassword.disabled = true;\n"
		"				document.login.login.disabled = true;\n"
		"				document.login.create.disabled = true;\n"
		"				document.login.logout.disabled = false;\n"
		"				document.login.abort.disabled = true;\n"
		"				if (document.login.password.value == '')\n"
		"					document.login.password.value = '********';\n"
		"				document.getElementById('status').innerHTML = '["));
	RETURN_IF_ERROR(line.Append(success));
	RETURN_IF_ERROR(line.Append("]';\n"
		"			}\n"
		"			if (currentOp == '' || account.authState == 1)\n"
		"				document.getElementById('message').innerHTML = '';\n"
		"			else if (account.authStatus == account.OK)\n"
		"				document.getElementById('message').innerHTML = currentOp+': "));
	RETURN_IF_ERROR(line.Append(success));
	RETURN_IF_ERROR(line.Append("';\n"
		"			else\n"
		"				document.getElementById('message').innerHTML = currentOp+': '+account.authStatus;\n"
		"			if (account.authState != 1)\n"
		"				currentOp = '';\n"
		"		}\n"
		"		function doLogin() {\n"
		"			var username = document.login.username.value;\n"
		"			var password = document.login.password.value;\n"
		"			var savePassword = document.login.savePassword.checked;\n"
		"			currentOp = 'Login';\n"
		"			account.login(username, password, savePassword);\n"
		"		}\n"
		"		function doCreateAccount() {\n"
		"			var username = document.login.username.value;\n"
		"			var password = document.login.password.value;\n"
		"			var email = document.login.email.value;\n"
		"			var savePassword = document.login.savePassword.checked;\n"
		"			currentOp = 'Create account';\n"
		"			account.createAccount(username,password,email,savePassword);\n"
		"		}\n"
		"		function doLogout() {\n"
		"			currentOp = 'Logout';\n"
		"			document.login.password.value = '';\n"
		"			account.logout();\n"
		"		}\n"
		"		function doAbort() {\n"
		"			account.abort();\n"
		"		}\n"
		"	</script>\n"
		"	<form name='login'>\n"
		"		<p><label>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::D_FEATURE_USERNAME));
	RETURN_IF_ERROR(line.Append(": <input type='text' name='username';'></label>\n"
		"			<label>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::D_FEATURE_PASSWORD));
	RETURN_IF_ERROR(line.Append(": <input type='password' name='password'></label></p>\n"
		"		<p><label>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::D_FEATURE_EMAIL));
	RETURN_IF_ERROR(line.Append(": <input type='text' name='email'></label>\n"
		"			<label>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_REMEMBER_LOGIN));
	RETURN_IF_ERROR(line.Append(": <input type='checkbox' name='savePassword' checked='true'></label></p>\n"
		"		<p><input type='button' name='login' value='"));
	RETURN_IF_ERROR(line.Append(log_in));
	RETURN_IF_ERROR(line.Append("' onclick='doLogin();'>\n"
		"			<input type='button' name='create' value='"));
	RETURN_IF_ERROR(line.Append(create_account));
	RETURN_IF_ERROR(line.Append("' onclick='doCreateAccount();'>\n"
		"			<input type='button' name='logout' value='"));
	RETURN_IF_ERROR(line.Append(log_out));
	RETURN_IF_ERROR(line.Append("' onclick='doLogout();'>\n"
		"			<input type='button' name='abort' value='"));
	RETURN_IF_ERROR(line.Append(cancel));
	RETURN_IF_ERROR(line.Append("' onclick = 'doAbort();'></p>\n"
		"		<p><span id='status'></span> <span id='message'></span></p>\n"
		"	</form>\n"
		"</div>\n"
		"<div id='footer'>\n"
		"	<a href='http://unite.opera.com/applications/'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::M_WEBSERVER_NEW_SERVICE));
	RETURN_IF_ERROR(line.Append("</a>\n"
		"</div>\n"
		"<div id='overlay'><div id='dwrapper'><div id='dtitle'></div><div id='info'></div></div></div>\n"
	));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	RETURN_IF_ERROR(CloseDocument());

	return OpStatus::OK;
}

#endif // OPERAUNITE_URL
