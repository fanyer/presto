/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/about/operawebstorage.h"

#ifdef HAS_OPERA_WEBSTORAGE_PAGE

#include "modules/database/opdatabase.h"
#include "modules/database/opdatabasemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/pi/OpLocale.h"
#include "modules/url/url2.h"
#include "modules/util/htmlify.h"

#define NVL(a, b) ((a) != NULL ? (a) : (b))

//to control the select box
#define HANDLING_SELECT "        <option %s value=1>%s</option><option %s value=2>%s</option>\n<option %s value=0>%s</option>\n"
#define HANDLING_TO_STR(handling, val) ((handling) == (PS_Policy::val) ? UNI_L("selected") : UNI_L(""))
#define HANDLING_BLOCK(var) HANDLING_TO_STR(var, KQuotaAskUser),quota_ask_user.CStr(),HANDLING_TO_STR(var, KQuotaAllow),quota_allow.CStr(),HANDLING_TO_STR(var, KQuotaDeny),quota_deny.CStr()

#ifdef DATABASE_ABOUT_WEBDATABASES_URL
OP_STATUS OperaWebStorage::GenerateWebSQLDatabasesData()
{
	PS_IndexIterator * OP_MEMORY_VAR it = NULL;
	RETURN_IF_LEAVE(it = g_database_manager->GetIteratorL(m_url.GetContextId(),
			PS_ObjectTypes::KWebDatabases, TRUE, PS_IndexIterator::ORDERED_ASCENDING));
	OpStackAutoPtr<PS_IndexIterator> it_ptr(it);

	PS_IndexEntry * OP_MEMORY_VAR key = NULL;
	RETURN_IF_LEAVE(key = it->GetItemL());
	if (!key)
		return OpStatus::OK;

	const uni_char * OP_MEMORY_VAR prev_origin = NULL;
	const uni_char * OP_MEMORY_VAR str_to_htmlify = NULL;
	uni_char * OP_MEMORY_VAR htmlified_str = NULL;

	unsigned max_size;
	int handling;
	BOOL has_configurable_fields;
	TempBuffer line;

	RETURN_IF_ERROR(Heading(Str::S_WEBDATABASES_DATABASES));

	OpString click_to_expand, max_on_disk, when_max_exceeds, quota_ask_user,
	         quota_allow, quota_deny, save, delete_all, database_name,
	         size_header, to_be_deleted, data_file_path, delete_str, kbytes,
	         error_access_file, file_not_saved;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CLICK_TO_EXPAND, click_to_expand));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAX_SIZE_ON_DISK, max_on_disk));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_WHEN_MAX_EXCEEDS, when_max_exceeds));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_QUOTA_ASK_USER, quota_ask_user));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_QUOTA_ALLOW, quota_allow));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_QUOTA_DENY, quota_deny));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_SAVE_BUTTON_TEXT, save));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DELETE_ALL, delete_all));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DATABASE_NAME, database_name));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_DIRECTORY_COLOUMN_SIZE, size_header));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_TO_BE_DELETED, to_be_deleted));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DATA_FILE_PATH, data_file_path));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DELETE, delete_str));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_KILOBYTE, kbytes));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ERROR_ACCESSING_FILE, error_access_file));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_FILE_NOT_SAVED_YET, file_not_saved));

	while (key)
	{
		/* Empty origins occur in selftests, a way to check that's the only case would be good to have. */
		const uni_char *origin = NVL(key->GetOrigin(), UNI_L(""));
		const uni_char *domain = NVL(key->GetDomain(), UNI_L(""));

		max_size = (unsigned)(key->GetPolicyAttribute(PS_Policy::KOriginQuota, NULL) / 1024);
		handling = key->GetPolicyAttribute(PS_Policy::KOriginExceededHandling, NULL);
		has_configurable_fields = FALSE;

		if (!prev_origin || uni_strcmp(prev_origin, origin))
		{
			//this closes the previous section
			if (prev_origin)
			{
				m_section_id++;
				RETURN_IF_ERROR(line.Append("</table></div>\n"));
			}

			prev_origin = origin;

			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"<h3 class=\"show-hide-toggle\" title=\"%s\" onclick=\"return show_hide(%d, event);\"><a href=\"%s\">%s</a></h3>\n<div id=\"section-%d\" style=\"display:none\"><table>\n"),
					click_to_expand.CStr(), m_section_id,
					*prev_origin ? prev_origin : UNI_L("#"),
					*prev_origin ? prev_origin : UNI_L("&lt;no-domain&gt;"), m_section_id));

			RETURN_IF_ERROR(line.AppendFormat(UNI_L("  <tr>\n    <td>%s</td>\n    <td>\n"),
					max_on_disk.CStr()));

			if (key->IsPolicyConfigurable(PS_Policy::KOriginQuota))
			{
				has_configurable_fields = TRUE;
				RETURN_IF_ERROR(line.AppendFormat(UNI_L(
						"      <input id=\"quota-%d\" type=\"number\" class=\"integer\" step=\"10\" value=\"%u\" orig-value=\"%u\">\n"),
						m_section_id, max_size, max_size));
			}
			else
			{
				RETURN_IF_ERROR(line.AppendFormat(UNI_L("      %u %s\n"), max_size, kbytes.CStr()));
			}

			RETURN_IF_ERROR(line.AppendFormat(UNI_L("    </td>\n  </tr>\n  <tr>\n    <td>%s</td>\n    <td>\n"),
				when_max_exceeds.CStr()));

			if (key->IsPolicyConfigurable(PS_Policy::KOriginExceededHandling))
			{
				has_configurable_fields = TRUE;
				RETURN_IF_ERROR(line.AppendFormat(UNI_L(
						"      <select id=\"handling-%d\" orig-value=\"%d\">\n"), m_section_id, handling));
				RETURN_IF_ERROR(line.AppendFormat(UNI_L(HANDLING_SELECT), HANDLING_BLOCK(handling)));
				RETURN_IF_ERROR(line.Append("      </select>\n"));
			}
			else
			{
				switch(handling)
				{
				case PS_Policy::KQuotaDeny:
					RETURN_IF_ERROR(line.Append(quota_deny));
					break;
				case PS_Policy::KQuotaAskUser:
					RETURN_IF_ERROR(line.Append(quota_ask_user));
					break;
				case PS_Policy::KQuotaAllow:
					RETURN_IF_ERROR(line.Append(quota_allow));
					break;
				}
			}

			RETURN_IF_ERROR(line.Append("\n      </td>\n  </tr>\n  <tr>\n"));

			if (has_configurable_fields)
			{
				RETURN_IF_ERROR(line.AppendFormat(UNI_L(
						"    <td><button onclick=\"save_preferences(%d, 'databases', '%s')\">%s</button></td>\n"),
						m_section_id, domain, save.CStr()));
			}
			else
			{
				RETURN_IF_ERROR(line.Append("    <td> </td>\n"));
			}

			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"    <td><button onclick=\"delete_all_dbs('%s')\">%s</button></td>\n"),
					NVL(key->GetOrigin(),UNI_L("")),delete_all.CStr()));
			RETURN_IF_ERROR(line.AppendFormat(UNI_L("  </tr>\n</table>\n<table>\n  <thead>\n    <tr>\n      <th>%s</th>\n      <th>%s</th>\n      <th></th>\n      <th>%s</th>\n    </tr>\n  </thead>\n"),
					database_name.CStr(), size_header.CStr(), data_file_path.CStr()));
		}

		OpFileLength size;
		RETURN_IF_ERROR(key->GetDataFileSize(&size));

		str_to_htmlify = NVL(key->GetName(), UNI_L(""));
		RETURN_OOM_IF_NULL(htmlified_str = HTMLify_string(str_to_htmlify, uni_strlen(str_to_htmlify), TRUE));

		ANCHOR_ARRAY(uni_char,htmlified_str);
		RETURN_IF_ERROR(line.AppendFormat(UNI_L(
				"  <tr>\n    <td>%s%s</td>\n    <td>%.1f %s</td>\n"),
				htmlified_str, key->WillBeDeleted() ? to_be_deleted.CStr() : UNI_L(""), size / 1024.0, kbytes.CStr()));
		RETURN_IF_ERROR(line.AppendFormat(
			UNI_L("    <td><button onclick=\"opera.deleteDatabase('%s', '%s')\">%s</button></td>\n"),
			htmlified_str, origin, delete_str.CStr()));
		ANCHOR_ARRAY_DELETE(htmlified_str);

		if (key->GetFileNameObj() && key->GetFileNameObj()->IsBogus())
		{
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"    <td class=\"error\"><em>&lt;%s&gt;</em></td>\n  </tr>\n"),
					error_access_file.CStr()));
		}
		else if (key->GetFileRelPath())
		{
			str_to_htmlify = NVL(key->GetFileRelPath(), UNI_L(""));
			RETURN_OOM_IF_NULL(htmlified_str = HTMLify_string(str_to_htmlify, uni_strlen(str_to_htmlify), TRUE));

			ANCHOR_ARRAY_RESET(htmlified_str);
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"    <td>%s</td>\n  </tr>\n"),
					htmlified_str));
			ANCHOR_ARRAY_DELETE(htmlified_str);
		}
		else
		{
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"    <td><em>&lt;%s&gt;</em></td>\n  </tr>\n"),
					file_not_saved.CStr()));
		}

		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.GetStorage()));
		line.Clear();

		RETURN_IF_LEAVE(it->MoveNextL());
		RETURN_IF_LEAVE(key = it->GetItemL());
	}

	m_section_id++;
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</table></div>\n")));

	return OpStatus::OK;
}
#endif // DATABASE_ABOUT_WEBDATABASES_URL

#ifdef DATABASE_ABOUT_WEBSTORAGE_URL

//this is temporary because currently there's no way to iterate
//over webstorage objects without relying on the database APIs
OP_STATUS OperaWebStorage::GenerateWebStorageData(WebStorageType ws_t,
		Str::LocaleString title, const uni_char* clear_fn, const uni_char* prefs_obj_name)
{
	PS_IndexIterator * OP_MEMORY_VAR it = NULL;
	RETURN_IF_LEAVE(it = g_database_manager->GetIteratorL(m_url.GetContextId(),
			WebStorageTypeToPSObjectType(ws_t), TRUE, PS_IndexIterator::ORDERED_ASCENDING));
	OpStackAutoPtr<PS_IndexIterator> it_ptr(it);

	const uni_char * OP_MEMORY_VAR str_to_htmlify = NULL;
	uni_char * OP_MEMORY_VAR htmlified_str = NULL;

	PS_IndexEntry * OP_MEMORY_VAR key = NULL;
	OpFileLength size;
	unsigned max_size;
	int handling;
	BOOL has_configurable_fields;
	TempBuffer line;

	RETURN_IF_LEAVE(key = it->GetItemL());

	//if the storage area is empty, we just quit !
	//this way, a normal session will not see widget.preferences content
	if (!key)
		return OpStatus::OK;

	RETURN_IF_ERROR(Heading(title));

	OpString click_to_expand, size_header, max_on_disk, when_max_exceeds,
	         quota_ask_user, quota_allow, quota_deny, save, data_file_path,
	         clear_database, bytes, kbytes, error_access_file, file_not_saved;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CLICK_TO_EXPAND, click_to_expand));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAX_SIZE_ON_DISK, max_on_disk));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_WHEN_MAX_EXCEEDS, when_max_exceeds));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_QUOTA_ASK_USER, quota_ask_user));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_QUOTA_ALLOW, quota_allow));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_QUOTA_DENY, quota_deny));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_SAVE_BUTTON_TEXT, save));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_DIRECTORY_COLOUMN_SIZE, size_header));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DATA_FILE_PATH, data_file_path));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CLEAR_DATABASE, clear_database));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_BYTES, bytes));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_KILOBYTE, kbytes));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ERROR_ACCESSING_FILE, error_access_file));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_FILE_NOT_SAVED_YET, file_not_saved));

	while (key)
	{
		const uni_char *origin = NVL(key->GetOrigin(), UNI_L(""));
		const uni_char *domain = NVL(key->GetDomain(), UNI_L(""));

		max_size = (unsigned)(key->GetPolicyAttribute(PS_Policy::KOriginQuota, NULL) / 1024);
		handling = key->GetPolicyAttribute(PS_Policy::KOriginExceededHandling, NULL);
		has_configurable_fields = FALSE;

		RETURN_IF_ERROR(key->GetDataFileSize(&size));

		uni_char *origin_html = NULL;
		ANCHOR_ARRAY(uni_char, origin_html);

		if (*origin)
		{
			RETURN_OOM_IF_NULL(origin_html = HTMLify_string(origin, uni_strlen(origin), TRUE));
			ANCHOR_ARRAY_RESET(origin_html);
		}

		RETURN_IF_ERROR(line.AppendFormat(UNI_L(
				"<h3 class=\"show-hide-toggle\" title=\"%s\" onclick=\"return show_hide(%d, event);\"><a href=\"%s\">%s</a></h3>\n<div id=\"section-%d\" style=\"display:none\"><table>\n"),
				click_to_expand.CStr(), m_section_id,
				origin_html ? origin_html : UNI_L("#"),
				origin_html ? origin_html : UNI_L("&lt;no-domain&gt;"), m_section_id));

		if (size >= 1024)
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
				"  <tr>\n    <td>%s</td>\n    <td>%.1f %s</td>\n"),
				size_header.CStr(), size / 1024.0, kbytes.CStr()));
		else
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
				"  <tr>\n    <td>%s</td>\n    <td>%u %s</td>\n"),
				size_header.CStr(), (unsigned)size, bytes.CStr()));

		RETURN_IF_ERROR(line.AppendFormat(UNI_L("  </tr>\n  <tr>\n    <td>%s</td>\n"),
				data_file_path.CStr()));

		if (key->GetFileNameObj() && key->GetFileNameObj()->IsBogus())
		{
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
				"    <td><em>&lt;%s&gt;</em></td>\n"),
				error_access_file.CStr()));
		}
		else if (key->GetFileRelPath())
		{
			str_to_htmlify = NVL(key->GetFileRelPath(), UNI_L(""));
			RETURN_OOM_IF_NULL(htmlified_str = HTMLify_string(str_to_htmlify, uni_strlen(str_to_htmlify), TRUE));
			ANCHOR_ARRAY(uni_char, htmlified_str);
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"    <td>%s</td>\n"),
					htmlified_str));
			ANCHOR_ARRAY_DELETE(htmlified_str);
		}
		else
		{
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
				"    <td><em>&lt;%s&gt;</em></td>\n"),
				file_not_saved.CStr()));
		}

		RETURN_IF_ERROR(line.AppendFormat(UNI_L("  </tr>\n  <tr>\n    <td>%s</td>\n    <td>\n"),
				max_on_disk.CStr()));

		if (key->IsPolicyConfigurable(PS_Policy::KOriginQuota))
		{
			has_configurable_fields = TRUE;
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"      <input id=\"quota-%d\" type=\"number\" class=\"integer\" step=\"10\" value=\"%u\" orig-value=\"%u\">\n"),
					m_section_id, max_size, max_size));
		}
		else
		{
			RETURN_IF_ERROR(line.AppendFormat(UNI_L("      %u %s\n"), max_size, kbytes.CStr()));
		}

		RETURN_IF_ERROR(line.AppendFormat(UNI_L("    </td>\n  </tr>\n  <tr>\n    <td>%s</td>\n    <td>\n      "),
			when_max_exceeds.CStr()));

		if (key->IsPolicyConfigurable(PS_Policy::KOriginExceededHandling))
		{
			has_configurable_fields = TRUE;
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"<select id=\"handling-%d\" orig-value=\"%d\">\n"), m_section_id, handling));
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(HANDLING_SELECT), HANDLING_BLOCK(handling)));
			RETURN_IF_ERROR(line.Append("      </select>\n"));
		}
		else
		{
			switch(handling)
			{
			case PS_Policy::KQuotaDeny:
				RETURN_IF_ERROR(line.Append(quota_deny));
				break;
			case PS_Policy::KQuotaAskUser:
				RETURN_IF_ERROR(line.Append(quota_ask_user));
				break;
			case PS_Policy::KQuotaAllow:
				RETURN_IF_ERROR(line.Append(quota_allow));
				break;
			}
		}

		RETURN_IF_ERROR(line.Append("\n    </td>\n  </tr>\n  <tr>\n"));

		if (has_configurable_fields)
		{
			RETURN_IF_ERROR(line.AppendFormat(UNI_L(
					"    <td><button onclick=\"save_preferences(%d, '%s', '%s')\">%s</button></td>\n"),
					m_section_id, prefs_obj_name, domain, save.CStr()));
		}
		else
		{
			RETURN_IF_ERROR(line.Append("    <td> </td>\n"));
		}

		RETURN_IF_ERROR(line.AppendFormat(UNI_L(
				"    <td><button storage-origin=\"%s\" onclick=\"opera.%s(this.getAttribute('storage-origin'))\">%s</button></td>\n"),
				origin_html, clear_fn, clear_database.CStr()));

		RETURN_IF_ERROR(line.Append("  </tr>\n</table></div>\n"));
		ANCHOR_ARRAY_DELETE(origin_html);

		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.GetStorage()));
		line.Clear();

		m_section_id++;
		RETURN_IF_LEAVE(it->MoveNextL());
		RETURN_IF_LEAVE(key = it->GetItemL());
	}

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</table>\n")));

	return OpStatus::OK;
}
#endif // DATABASE_ABOUT_WEBSTORAGE_URL

# define AB_WS_SCRIPT_BLOCK_1 \
"  <script type=\"text/javascript\">\n\
	function pref_name(obj_type, pref_type){\n\
		switch(obj_type)\n\
		{\n"
# define AB_WS_SCRIPT_BLOCK_2 \
"		case 'databases':\n\
			switch(pref_type){\n\
			case 'originquota':\n\
				return 'Domain Quota For Databases';\n\
			case 'quotahandling':\n\
				return 'Domain Quota Exceeded Handling For Databases';\n\
			}\n\
			break;\n"
# define AB_WS_SCRIPT_BLOCK_3 \
"		case 'localStorage':\n\
			switch(pref_type){\n\
			case 'originquota':\n\
				return 'Domain Quota For localStorage';\n\
			case 'quotahandling':\n\
				return 'Domain Quota Exceeded Handling For localStorage';\n\
			}\n\
			break;\n"
# define AB_WS_SCRIPT_BLOCK_4 \
"		case 'widgetPreferences':\n\
			switch(pref_type){\n\
			case 'originquota':\n\
				return 'Domain Quota For Widget Preferences';\n\
			case 'quotahandling':\n\
				return 'Domain Quota Exceeded Handling For Widget Preferences';\n\
			}\n\
			break;\n"
# define AB_WS_SCRIPT_BLOCK_5 \
"		case 'userJsStorage':\n\
			switch(pref_type){\n\
			case 'originquota':\n\
				return 'User JS Storage Quota';\n\
			}\n\
			break;\n"
#define AB_WS_SCRIPT_BLOCK_6a \
"		default: \n\
		}\n\
		throw new Error('bad arguments');\n\
	}\n\
	function save_preferences(section_id, type, domain) {\n\
		var quota_inp = document.getElementById('quota-'+section_id);\n\
		if (quota_inp.value != quota_inp.getAttribute('orig-value')){\n\
			var quota_pref = pref_name(type, 'originquota');\n\
			if(!quota_pref) return;\n\
			quota_inp.setAttribute('orig-value', quota_inp.value);\n\
			opera.setPreference('Persistent Storage', quota_pref, quota_inp.value, domain);\n\
		}\n\
		var handling_sel = document.getElementById('handling-'+section_id);\n\
		if (handling_sel.value != handling_sel.getAttribute('orig-value')){\n\
			var handling_pref = pref_name(type, 'quotahandling');\n\
			if(!handling_pref) return;\n\
			handling_sel.setAttribute('orig-value', quota_inp.value);\n\
			opera.setPreference('Persistent Storage', handling_pref, handling_sel.value, domain);\n\
		}\n\
	}\n\
	function delete_all_dbs(origin){\n\
		if (confirm(origin?"
#define AB_WS_SCRIPT_BLOCK_6b "))\n\
			opera.deleteDatabase('*', origin);\n\
	}\n\
	var toggled_section_id;\n\
	function show_hide(section_id, event){\n\
		if (event.ctrlKey && event.shiftKey)\n\
			return;\n\
		var section = toggled_section_id != undefined ? document.getElementById('section-' + toggled_section_id) : null;\n\
		if (section){\n\
			var old_doc_heigth = document.body.offsetHeight;\n\
			section.style.display = section.style.display == 'none' ? '' : 'none';\n\
			if (toggled_section_id == section_id){\n\
				toggled_section_id = undefined;\n\
				return false;\n\
			}\n\
			else if (toggled_section_id <= section_id){\n\
				var height_diff = old_doc_heigth - document.body.offsetHeight;\n\
				height_diff = Math.min(height_diff, document.documentElement.scrollHeight - document.documentElement.scrollTop - innerHeight);\n\
				scrollBy(0, -height_diff);\n\
			}\n\
		}\n\
		toggled_section_id = undefined;\n\
		section = document.getElementById('section-' + section_id);\n\
		if (section){\n\
			toggled_section_id = section_id;\n\
			section.style.display = '';\n\
		}\n\
		return false;\n\
	}\n\
  </script>"

OP_STATUS OperaWebStorage::GenerateData()
{
	//main title
	Str::LocaleString title = Str::NOT_A_STRING;
#ifdef DATABASE_ABOUT_WEBDATABASES_URL
	if (m_type == WEB_DATABASES)
	{
		title = Str::S_WEBDATABASES_TITLE;
	}
#endif // DATABASE_ABOUT_WEBDATABASES_URL
#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	if (m_type == WEB_STORAGE)
	{
		title = Str::S_WEBSTORAGE_TITLE;
	}
#endif // DATABASE_ABOUT_WEBSTORAGE_URL

	//open document
	RETURN_IF_ERROR(OpenDocument(title, PrefsCollectionFiles::StyleWebStorageFile));

	//convert the script to UTF-16
	OpString script;
	RETURN_IF_ERROR(script.Set(AB_WS_SCRIPT_BLOCK_1));

#ifdef DATABASE_ABOUT_WEBDATABASES_URL
	if (m_type == WEB_DATABASES)
	{
		RETURN_IF_ERROR(script.Append(AB_WS_SCRIPT_BLOCK_2));
	}
#endif // DATABASE_ABOUT_WEBDATABASES_URL

#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	if (m_type == WEB_STORAGE)
	{
		RETURN_IF_ERROR(script.Append(AB_WS_SCRIPT_BLOCK_3));

# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
		RETURN_IF_ERROR(script.Append(AB_WS_SCRIPT_BLOCK_4));
# endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT

# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
		RETURN_IF_ERROR(script.Append(AB_WS_SCRIPT_BLOCK_5));
# endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	}
#endif // DATABASE_ABOUT_WEBSTORAGE_URL
	RETURN_IF_ERROR(script.Append(AB_WS_SCRIPT_BLOCK_6a));

	OpString delete_all, delete_all_for_origin, cannot_undo;
	RETURN_IF_ERROR(JSSafeLocaleString(&delete_all, Str::S_DELETE_ALL_DATABASES));
	RETURN_IF_ERROR(JSSafeLocaleString(&delete_all_for_origin, Str::S_DELETE_ALL_DATABASES_FOR_ORIGIN));
	RETURN_IF_ERROR(JSSafeLocaleString(&cannot_undo, Str::S_CANNOT_UNDO));

	RETURN_IF_ERROR(script.AppendFormat(UNI_L("'%s':('%s\\n\"'+origin+'\"\\n')+'%s'"),
		delete_all.CStr(), delete_all_for_origin.CStr(), cannot_undo.CStr()));
	RETURN_IF_ERROR(script.Append(AB_WS_SCRIPT_BLOCK_6b));

	//writing the script
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, script.CStr()));
	script.Empty();

	RETURN_IF_ERROR(OpenBody(title));

	//writing sections
# ifdef DATABASE_ABOUT_WEBDATABASES_URL
	if (m_type == WEB_DATABASES)
	{
		RETURN_IF_ERROR(GenerateWebSQLDatabasesData());
	}
# endif // DATABASE_ABOUT_WEBDATABASES_URL

#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	if (m_type == WEB_STORAGE)
	{
		RETURN_IF_ERROR(GenerateWebStorageData(WEB_STORAGE_LOCAL,
				Str::S_WEBSTORAGE_LOCALSTORAGE, UNI_L("clearLocalStorage"), UNI_L("localStorage")));
# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
		RETURN_IF_ERROR(GenerateWebStorageData(WEB_STORAGE_WGT_PREFS,
				Str::S_WEBSTORAGE_WIDGETPREFERENCES, UNI_L("clearWidgetPreferences"), UNI_L("widgetPreferences")));
# endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT

# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
		RETURN_IF_ERROR(GenerateWebStorageData(WEB_STORAGE_USERJS, Str::S_WEBSTORAGE_USERJSSTORAGE, UNI_L("clearScriptStorage"), UNI_L("userJsStorage")));
# endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	}
#endif // DATABASE_ABOUT_WEBSTORAGE_URL

	if (m_section_id == 0)
	{
		RETURN_IF_ERROR(Heading(Str::S_WEBSTORAGE_NO_DATA_FOUND));
	}

	RETURN_IF_ERROR(GeneratedByOpera());

	return CloseDocument();
}

#endif // HAS_OPERA_WEBSTORAGE_PAGE
