/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef OPERABOOKMARKS_URL

#include "modules/util/opstring.h"
#include "modules/util/simset.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/bookmark_item.h"
#include "modules/locale/locale-enum.h"

#include "modules/bookmarks/operabookmarks.h"

OP_STATUS OperaBookmarks::GenerateData()
{
	OpString line, url, title, str;
	BookmarkAttribute attr;
	BookmarkListElm *elm;
	AutoDeleteHead bookmark_list;
	OpString url_text, title_text, desc_text, sn_text, folder_text, add_text, add_folder_text, load_text, save_text, save_mod_text, modify_text, delete_text;

#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::SI_IDSTR_HL_BOOKMARKS_ROOT_FOLDER_NAME, PrefsCollectionFiles::StyleBookmarksFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::SI_IDSTR_HL_BOOKMARKS_ROOT_FOLDER_NAME));
#endif // _LOCALHOST_SUPPORT_

	RETURN_IF_ERROR(line.Set("<script>\n"
			 "function update()\n"
			 "{\n"
			 "var url = document.getElementById('au').value;\n"
			 "var title = document.getElementById('at').value;\n"
			 "var desc = document.getElementById('ad').value;\n"
			 "var sn = document.getElementById('as').value;\n"
			 "var pt=document.getElementById('ap').value;\n"
			 "opera.bookmarksSaveFormValues(url,title,desc,sn,pt);\n"
			 "location.reload();\n"
			 "}\n"
			 "function sv()\n"
			 "{\n"
			 "var url = opera.bookmarksGetFormUrlValue();\n"
			 "var title = opera.bookmarksGetFormTitleValue();\n"
			 "var desc = opera.bookmarksGetFormDescriptionValue();\n"
			 "var sn = opera.bookmarksGetFormShortnameValue();\n"
			 "var pt = opera.bookmarksGetFormParentTitleValue();\n"
			 "if (url && url!='undefined')"
			 "document.getElementById('au').value = url;\n"
			 "if (title && title!='undefined')"
			 "document.getElementById('at').value = title;\n"
			 "if (desc && desc!='undefined')"
			 "document.getElementById('ad').value = desc;\n"
			 "if (sn && sn!='undefined')"
			 "document.getElementById('as').value = sn;\n"
			 "if (pt && pt!='undefined')"
			 "document.getElementById('ap').value = pt;\n"
			 "}\n"
			 "function clearf()\n"
			 "{\n"
			 "opera.bookmarksSaveFormValues('','','','','');\n"
			 "document.getElementById('au').value = '';\n"
			 "document.getElementById('at').value = '';\n"
			 "document.getElementById('ad').value = '';\n"
			 "document.getElementById('as').value = '';\n"
			 "document.getElementById('ap').value = '';\n"
			 "}\n"
			 "opera.setBookmarkListener(update);\n"
			 "document.addEventListener('load', sv, false);"
			 "function mod(elm)"
			 "{"
			 "document.getElementById('au').setAttribute('uid', elm.getAttribute('uid'));"
			 "document.getElementById('au').value = elm.getAttribute('url');"
			 "document.getElementById('at').value = elm.getAttribute('t');"
			 "document.getElementById('ad').value = elm.getAttribute('d');"
			 "document.getElementById('as').value = elm.getAttribute('sn');"
			 "document.getElementById('ap').value = elm.getAttribute('pt');"
			 "}"
			 "function del(b)"
			 "{"
			 "opera.deleteBookmark(b.getAttribute('uid'));"
			 "update();"
			 "}"

			 "</script>"));
	m_url.WriteDocumentData(URL::KNormal, line);

	RETURN_IF_ERROR(OpenBody(Str::SI_IDSTR_HL_BOOKMARKS_ROOT_FOLDER_NAME));

	if (!line.Reserve(256))
		return OpStatus::ERR_NO_MEMORY;
	if (!url.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;
	if (!title.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;

    // FIXME: Should be loaded using oplanguagemanager
	RETURN_IF_ERROR(url_text.Set("URL"));
	RETURN_IF_ERROR(title_text.Set("Title"));
	RETURN_IF_ERROR(desc_text.Set("Description"));
	RETURN_IF_ERROR(sn_text.Set("Shortname"));
	RETURN_IF_ERROR(folder_text.Set("Folder"));
	RETURN_IF_ERROR(add_text.Set("Add bookmark"));
	RETURN_IF_ERROR(add_folder_text.Set("Add folder"));
	RETURN_IF_ERROR(load_text.Set("Load"));
	RETURN_IF_ERROR(save_text.Set("Save"));
	RETURN_IF_ERROR(save_mod_text.Set("Save changes"));
	RETURN_IF_ERROR(modify_text.Set("Modify"));
	RETURN_IF_ERROR(delete_text.Set("Delete"));
	
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, url_text));
	RETURN_IF_ERROR(line.Set(": <input type=\"text\" class=\"string\" maxlength="));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("%u id=\"au\" uid=\"\" value=\"\"></p>"), g_bookmark_manager->GetAttributeMaxLength(BOOKMARK_URL)));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, title_text));
	RETURN_IF_ERROR(line.Set(": <input type=\"text\" class=\"string\" maxlength="));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("%u id=\"at\" value=\"\"></p>"), g_bookmark_manager->GetAttributeMaxLength(BOOKMARK_TITLE)));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, desc_text));
	RETURN_IF_ERROR(line.Set(": <input type=\"text\" class=\"string\" maxlength="));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("%u id=\"ad\" value=\"\"></p>"), g_bookmark_manager->GetAttributeMaxLength(BOOKMARK_DESCRIPTION)));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, sn_text));
	RETURN_IF_ERROR(line.Set(": <input type=\"text\" class=\"string\" maxlength="));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("%u id=\"as\" value=\"\"></p>"), g_bookmark_manager->GetAttributeMaxLength(BOOKMARK_SHORTNAME)));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, folder_text));
	RETURN_IF_ERROR(line.Set(": <input type=\"text\" class=\"string\" maxlength="));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("%u id=\"ap\" value=\"\"></p>"), g_bookmark_manager->GetAttributeMaxLength(BOOKMARK_TITLE)));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<button type=\"button\" id=\"add_bookmark\" onclick=\"opera.addBookmark(document.getElementById('au').value, document.getElementById('at').value, document.getElementById('ad').value, document.getElementById('as').value, document.getElementById('ap').value); update(); clearf()\">")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, add_text));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</button>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<button type=\"button\" id=\"add_folder\" onclick=\"opera.addBookmarkFolder(document.getElementById('at').value, document.getElementById('ad').value, document.getElementById('as').value, document.getElementById('ap').value); update(); clearf();\">")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, add_folder_text));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</button>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<button type=\"button\" id=\"load_bookmarks\" onclick=\"opera.loadBookmarks(); clearf();\">")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, load_text));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</button>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<button type=\"button\" id=\"save_bookmarks\" onclick=\"opera.saveBookmarks(); clearf();\">")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, save_text));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</button>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<button type=\"button\" id=\"move\" onclick=\"opera.moveBookmark(document.getElementById('au').getAttribute('uid'), document.getElementById('au').value, document.getElementById('at').value, document.getElementById('ad').value, document.getElementById('as').value, document.getElementById('ap').value); update(); clearf();\">")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, save_mod_text));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</button>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<div id='bm'><ul>")));

	RETURN_IF_ERROR(g_bookmark_manager->GetList(&bookmark_list));

	UINT32 old_depth = 0;
	for (elm = (BookmarkListElm*) bookmark_list.First(); elm; elm = (BookmarkListElm*) elm->Suc())
	{
		UINT32 i;
		BookmarkItem *bookmark = elm->GetBookmark();
		if (bookmark->GetFolderType() == FOLDER_NO_FOLDER)
			i = bookmark->GetFolderDepth();
		else
			i = bookmark->GetFolderDepth()-1;

#ifdef CORE_SPEED_DIAL_SUPPORT
		if (bookmark->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER || bookmark->GetParentFolder()->GetFolderType() == FOLDER_SPEED_DIAL_FOLDER)
			continue;
#endif // CORE_SPEED_DIAL_SUPPORT

		for (; i<old_depth; i++)
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</ul>")));

		old_depth = elm->GetBookmark()->GetFolderDepth();

		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_URL, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(url));
		RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_TITLE, &attr));
		RETURN_IF_ERROR(attr.GetTextValue(title));

		OpString htmlified_title;
		if (!title.IsEmpty())
			RETURN_IF_ERROR(AppendHTMLified(&htmlified_title, title, title.Length()));

		line.Reserve(128);
		if (bookmark->GetFolderType() == FOLDER_SEPARATOR_FOLDER)
		{
			RETURN_IF_ERROR(line.Set("<hr>"));
		}
		else if (bookmark->GetFolderType() == FOLDER_NORMAL_FOLDER || bookmark->GetFolderType() == FOLDER_TRASH_FOLDER)
		{
			RETURN_IF_ERROR(line.Set("<div class=\"limited\"><li id=\""));
			RETURN_IF_ERROR(line.Append(bookmark->GetUniqueId()));
			RETURN_IF_ERROR(line.Append("\"><b>"));
			RETURN_IF_ERROR(line.Append(htmlified_title));
			RETURN_IF_ERROR(line.Append("</b></div>"));
			RETURN_IF_ERROR(line.Append("<button type=\"button\" id='del' uid=\""));
			RETURN_IF_ERROR(line.Append(bookmark->GetUniqueId()));
			RETURN_IF_ERROR(line.Append("\" onclick=\"del(this);\">"));
			RETURN_IF_ERROR(line.Append(delete_text));
			RETURN_IF_ERROR(line.Append("</button>"));

			// Add a modify button containing all modifyable bookmark attributes as attributes.
			RETURN_IF_ERROR(line.Append("<button type=\"button\" id='mod' uid=\""));
			RETURN_IF_ERROR(line.Append(bookmark->GetUniqueId()));
			RETURN_IF_ERROR(line.Append("\" t=\""));
			RETURN_IF_ERROR(line.Append(htmlified_title));
			RETURN_IF_ERROR(line.Append("\" d=\""));
			RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_DESCRIPTION, &attr));
			RETURN_IF_ERROR(attr.GetTextValue(str));
			if (!str.IsEmpty())
				RETURN_IF_ERROR(AppendHTMLified(&line, str, str.Length()));
			RETURN_IF_ERROR(line.Append("\" sn=\""));
			RETURN_IF_ERROR(bookmark->GetAttribute(BOOKMARK_SHORTNAME, &attr));
			RETURN_IF_ERROR(attr.GetTextValue(str));
			if (!str.IsEmpty())
				RETURN_IF_ERROR(AppendHTMLified(&line, str, str.Length()));
			RETURN_IF_ERROR(line.Append("\" pt=\""));
			if (bookmark->GetParentFolder() != g_bookmark_manager->GetRootFolder())
			{
				RETURN_IF_ERROR(elm->GetBookmark()->GetParentFolder()->GetAttribute(BOOKMARK_TITLE, &attr));
				RETURN_IF_ERROR(attr.GetTextValue(str));
				if (!str.IsEmpty())
					RETURN_IF_ERROR(AppendHTMLified(&line, str, str.Length()));
			}

			RETURN_IF_ERROR(line.Append("\" onclick=\"mod(this);\">"));
			RETURN_IF_ERROR(line.Append(modify_text));
			RETURN_IF_ERROR(line.Append(UNI_L("</button>")));
			RETURN_IF_ERROR(line.Append(UNI_L("</li><ul>")));
		}
		else
		{
			RETURN_IF_ERROR(line.Set("<div class=\"limited\"><li id=\""));
			RETURN_IF_ERROR(line.Append(elm->GetBookmark()->GetUniqueId()));
			RETURN_IF_ERROR(line.Append("\">"));
			if (url.HasContent())
			{
				RETURN_IF_ERROR(line.Append("<a href=\""));
				RETURN_IF_ERROR(AppendHTMLified(&line, url, url.Length()));
				RETURN_IF_ERROR(line.Append("\">"));
			}
			RETURN_IF_ERROR(line.Append(htmlified_title));
			RETURN_IF_ERROR(line.Append("</a></div><button type=\"button\" id='del' uid=\""));
			RETURN_IF_ERROR(line.Append(elm->GetBookmark()->GetUniqueId()));
			RETURN_IF_ERROR(line.Append("\" onclick=\"del(this);\">"));
			RETURN_IF_ERROR(line.Append(delete_text));
			RETURN_IF_ERROR(line.Append("</button>"));

			// Add a modify button containing all modifiable bookmark attributes as attributes.
			RETURN_IF_ERROR(line.Append("<button type=\"button\" id='mod' uid=\""));
			RETURN_IF_ERROR(line.Append(elm->GetBookmark()->GetUniqueId()));
			RETURN_IF_ERROR(line.Append("\" url=\""));
			RETURN_IF_ERROR(elm->GetBookmark()->GetAttribute(BOOKMARK_URL, &attr));
			RETURN_IF_ERROR(attr.GetTextValue(str));
			if (!str.IsEmpty())
				RETURN_IF_ERROR(AppendHTMLified(&line, str, str.Length()));
			RETURN_IF_ERROR(line.Append("\" t=\""));
			RETURN_IF_ERROR(line.Append(htmlified_title));
			RETURN_IF_ERROR(line.Append("\" d=\""));
			RETURN_IF_ERROR(elm->GetBookmark()->GetAttribute(BOOKMARK_DESCRIPTION, &attr));
			RETURN_IF_ERROR(attr.GetTextValue(str));
			if (!str.IsEmpty())
				RETURN_IF_ERROR(AppendHTMLified(&line, str, str.Length()));
			RETURN_IF_ERROR(elm->GetBookmark()->GetAttribute(BOOKMARK_SHORTNAME, &attr));
			RETURN_IF_ERROR(attr.GetTextValue(str));
			RETURN_IF_ERROR(line.Append("\" sn=\""));
			if (!str.IsEmpty())
				RETURN_IF_ERROR(AppendHTMLified(&line, str, str.Length()));
			RETURN_IF_ERROR(line.Append("\" pt=\""));
			if (elm->GetBookmark()->GetParentFolder() != g_bookmark_manager->GetRootFolder())
			{
				RETURN_IF_ERROR(elm->GetBookmark()->GetParentFolder()->GetAttribute(BOOKMARK_TITLE, &attr));
				RETURN_IF_ERROR(attr.GetTextValue(str));
				if (!str.IsEmpty())
					RETURN_IF_ERROR(AppendHTMLified(&line, str, str.Length()));
			}

			RETURN_IF_ERROR(line.Append("\" onclick=\"mod(this)\">"));
			RETURN_IF_ERROR(line.Append(modify_text));
			RETURN_IF_ERROR(line.Append("</button>"));
			RETURN_IF_ERROR(line.Append("</li>"));
		}

		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
	}

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</ul></div>")));
	return CloseDocument();
}

OperaBookmarksListener::OperaBookmarksListener()
{
	m_ai = NULL;
	m_callback = NULL;
	m_form_url = NULL;
	m_form_title = NULL;
	m_form_desc = NULL;
	m_form_sn = NULL;
	m_form_parent_title = NULL;
}

OperaBookmarksListener::~OperaBookmarksListener()
{
	OP_DELETEA(m_form_url);
	OP_DELETEA(m_form_title);
	OP_DELETEA(m_form_desc);
	OP_DELETEA(m_form_sn);
	OP_DELETEA(m_form_parent_title);
}

OP_STATUS OperaBookmarksListener::SetValue(uni_char **dst, const uni_char *src)
{
	unsigned len;
	if (src)
	{
		if (*dst)
			OP_DELETEA(*dst);
		len = uni_strlen(src)+1;
		*dst = OP_NEWA(uni_char, len);
		if (!*dst)
			return OpStatus::ERR_NO_MEMORY;
		uni_strcpy(*dst, src);
	}
	return OpStatus::OK;
}

void OperaBookmarksListener::SetSavedFormValues(const uni_char *form_url, const uni_char *form_title, const uni_char *form_desc, const uni_char *form_sn, const uni_char *form_parent_title)
{
	RETURN_VOID_IF_ERROR(SetValue(&m_form_url, form_url));
	RETURN_VOID_IF_ERROR(SetValue(&m_form_title, form_title));
	RETURN_VOID_IF_ERROR(SetValue(&m_form_desc, form_desc));
	RETURN_VOID_IF_ERROR(SetValue(&m_form_sn, form_sn));
	RETURN_VOID_IF_ERROR(SetValue(&m_form_parent_title, form_parent_title));
}

void OperaBookmarksListener::OnBookmarksLoaded(OP_STATUS ret, UINT32 operation_count)
{
	m_ai->CallFunction(m_callback, NULL, 0, NULL);
}

void OperaBookmarksListener::OnBookmarksSaved(OP_STATUS ret, UINT32 operation_count)
{
}

#ifdef SUPPORT_DATA_SYNC
void OperaBookmarksListener::OnBookmarksSynced(OP_STATUS ret)
{
	m_ai->CallFunction(m_callback, NULL, 0, NULL);
}
#endif // SUPPORT_DATA_SYNC

#endif // OPERABOOKMARKS_URL
