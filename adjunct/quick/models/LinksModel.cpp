/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "adjunct/quick/models/LinksModel.h"
#include "modules/doc/doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/layout/box/box.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/skin/OpSkinUtils.h"
#include "modules/viewers/viewers.h"
#include "modules/url/url2.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"


/***********************************************************************************
**
**	LinksModelItem
**
***********************************************************************************/

extern void CleanStringForDisplay(uni_char* string);	// from logdoc_util.cpp

OP_STATUS LinksModelItem::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == INIT_QUERY && m_frames_doc)
	{
		item_data->flags |= FLAG_INITIALLY_OPEN;
	}

	if (item_data->query_type != COLUMN_QUERY && item_data->query_type != MATCH_QUERY && item_data->query_type != INFO_QUERY)
	{
		return OpStatus::OK;
	}

	if (!m_init)
	{
		INT32 len = m_title.Length();
		BOOL valid = FALSE;
		BOOL was_space = FALSE;

		for (INT32 i = 0; i < len;)
		{
			if (m_title[i] < 0x20 || (was_space && m_title[i] == 0x20))
			{
				m_title.Delete(i, 1);
				len--;
			}
			else
			{
				if (m_title[i] == 0x20)
				{
					was_space = TRUE;
				}
				else
				{
					valid = TRUE;
					was_space = FALSE;
				}
				i++;
			}
		}

		if (!valid)
		{
			m_title.Empty();
		}

		UINT32 color_rgb = (m_font_color & OP_RGBA(0, 0, 0, 255)) | ((m_font_color & 0xff) << 16) | (m_font_color & 0xff00) | ((m_font_color >> 16) & 0xff);

		double h, s, v;

		OpSkinUtils::ConvertRGBToHSV(color_rgb, h, s, v);

		if (v >= 0.6)
		{
			v = 0.6;

			color_rgb = OpSkinUtils::ConvertHSVToRGB(h, s, v);

			m_font_color = (m_font_color & OP_RGBA(0, 0, 0, 255)) | ((color_rgb & 0xff) << 16) | (color_rgb & 0xff00) | ((color_rgb >> 16) & 0xff);
		}

		m_init = TRUE;
	}

	if (item_data->query_type == INFO_QUERY)
	{
		OpString title, address;

		g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, title);
		g_languageManager->GetString(Str::SI_LOCATION_TEXT, address);

		item_data->info_query_data.info_text->AddTooltipText(title.CStr(), m_title.CStr());
		if( m_url.Find(UNI_L("@")) == KNotFound )
		{
			item_data->info_query_data.info_text->AddTooltipText(address.CStr(), m_url.CStr());
			item_data->info_query_data.info_text->SetStatusText(m_url.CStr());
		}
		else
		{
			URL	url = g_url_api->GetURL(m_url.CStr());
			OpString url_string;
			url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_string);
			item_data->info_query_data.info_text->AddTooltipText(address.CStr(), url_string.CStr());
			item_data->info_query_data.info_text->SetStatusText(url_string.CStr());
		}
		return OpStatus::OK;
	}

	if (item_data->query_type == MATCH_QUERY)
	{
		if (m_title.FindI(*item_data->match_query_data.match_text) != KNotFound || m_url.FindI(*item_data->match_query_data.match_text) != KNotFound)
		{
			item_data->flags |= FLAG_MATCHED;
		}

		return OpStatus::OK;
	}

	switch (item_data->column_query_data.column)
	{
		case 0:
		{
			if( m_title.HasContent() )
			{
				item_data->column_query_data.column_text->Set(m_title.CStr());
			}
			else
			{
				if( m_url.Find(UNI_L("@")) == KNotFound )
				{
					item_data->column_query_data.column_text->Set(m_url.CStr());
				}
				else
				{
					URL	url = g_url_api->GetURL(m_url.CStr());
					url.GetAttribute(URL::KUniName_Username_Password_Hidden, *item_data->column_query_data.column_text);
				}
			}
			item_data->column_query_data.column_sort_order = GetIndex();

			if (m_title.IsEmpty())
			{
				item_data->column_query_data.column_sort_order += 100000;
			}

			if (item_data->flags & FLAG_NO_PAINT)
			{
				return OpStatus::OK;
			}

			item_data->column_query_data.column_text_color = m_font_color;

			if (m_font_weight > 4)
			{
				item_data->flags |= FLAG_BOLD;
			}

			const uni_char* extension = m_url.CStr() + m_url.FindLastOf('.') + 1;

			if (extension && uni_strlen(extension) == 3 && extension[2] != '/'
				&& uni_stricmp(extension, UNI_L("htm"))
				&& uni_stricmp(extension, UNI_L("dml"))
				&& uni_stricmp(extension, UNI_L("jsp"))
				&& uni_stricmp(extension, UNI_L("com")))
			{
				Viewer * viewer = g_viewers->FindViewerByExtension(extension);

				if (viewer)
				{
					const uni_char* type = viewer->GetContentTypeString();

					if (uni_strni_eq(type, "IMAGE/", 6))
					{
						item_data->column_query_data.column_image = "Attachment Images";
					}
					else if (uni_strni_eq(type, "AUDIO/", 6))
					{
						item_data->column_query_data.column_image = "Attachment Music";
					}
					else if (uni_strni_eq(type, "VIDEO/", 6))
					{
						item_data->column_query_data.column_image = "Attachment Video";
					}
					else if (!uni_stricmp(extension, UNI_L("exe")))
					{
						item_data->column_query_data.column_image = "Attachment Programs";
					}
					else if (!uni_stricmp(extension, UNI_L("zip")) ||
							 !uni_stricmp(extension, UNI_L("arj")) ||
							 !uni_stricmp(extension, UNI_L("rar")) )
					{
						item_data->column_query_data.column_image = "Attachment Archives";
					}
					else
					{
						item_data->column_query_data.column_image = "Attachment Documents";
					}
				}
			}
			else if( extension && uni_strlen(extension) == 2 && extension[1] != '/' )
			{
				if (g_viewers->FindViewerByExtension(extension))
				{
					if (!uni_stricmp(extension, UNI_L("gz")) )
					{
						item_data->column_query_data.column_image = "Attachment Archives";
					}
				}
			}

			if (m_frames_doc)
			{
				item_data->column_query_data.column_image = "Group";
				item_data->column_query_data.column_text_color = OP_RGB(0x78, 0x50, 0x28);
			}
			break;
		}

		case 1:
		{
			if( m_url.Find(UNI_L("@")) == KNotFound )
			{
				item_data->column_query_data.column_text->Set(m_url.CStr());
			}
			else
			{
				URL	url = g_url_api->GetURL(m_url.CStr());

				url.GetAttribute(URL::KUniName_Username_Password_Hidden, *item_data->column_query_data.column_text);
			}
			item_data->column_query_data.column_text_color = OP_RGB(0, 0, 0);
			break;
		}
	}

	return OpStatus::OK;
}

#ifdef WIC_CAP_URLINFO_REUSE
BOOL LinksModelItem::OnDone(URLInformation* url_info)
{
	return FALSE;
}
#endif // WIC_CAP_URLINFO_REUSE

LinksModelItem::~LinksModelItem()
{
#ifdef WIC_CAP_URLINFO_REUSE
	if (m_url_info)
		OP_DELETE(m_url_info);
#endif // WIC_CAP_URLINFO_REUSE	
}
/***********************************************************************************
**
**	LinksModel
**
***********************************************************************************/

void LinksModel::UpdateLinks(OpWindowCommander* window_commander, BOOL redraw)
{
	if (GetCount() <= 1)
	{
		redraw = FALSE;
	}

	Window* window = window_commander->GetWindow();

	if (window->GetCurrentDoc() && window->GetCurrentDoc()->Type() == DOC_FRAMES)
	{
		FramesDocument* frames_doc = (FramesDocument*) window->GetCurrentDoc();

		UpdateLinks(window_commander, frames_doc, redraw, -1, FALSE);
	}

	if (redraw)
	{
		ChangeAll();
	}
}

/***********************************************************************************
**
**	LinksModel
**
***********************************************************************************/

void CleanTextData(OpString& text)
{
	if(text.HasContent())
	{
		unsigned int len = text.Length() + 1;
		uni_char *tmpbuf = OP_NEWA(uni_char, len);
		if(tmpbuf)
		{
			uni_strlcpy(tmpbuf, text.CStr(), len);
			CleanStringForDisplay(tmpbuf);
			text.Set(tmpbuf);
			OP_DELETEA(tmpbuf);
		}
	}
}

void LinksModel::UpdateLinks(OpWindowCommander * windowcommander, FramesDocument* frames_doc, BOOL redraw, INT32 parent, BOOL in_sub_doc)
{
	// find LinksModelItem for this frame_doc.. 

	LinksModelItem* page = NULL;

	INT32 pos = GetChildIndex(parent);

	while (pos != -1)
	{
		LinksModelItem* child = GetItemByIndex(pos);

		if (!child->m_frames_doc)
		{
			break;
		}

		if (child->m_frames_doc == frames_doc)
		{
			page = child;
			break;
		}

		pos = GetSiblingIndex(pos);
	}

	// if frames doc not found, add it

	if (!page)
	{
		URL url = frames_doc->GetURL();
		ViewActionFlag view_action_flag;
		TransferManagerDownloadCallback * download_callback = OP_NEW(TransferManagerDownloadCallback, (frames_doc->GetDocManager(), url, NULL, view_action_flag));
		OpString url_string;
		url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_string, TRUE);
		page = OP_NEW(LinksModelItem, (windowcommander, download_callback, url_string.CStr()));
		if (!page)
		{
			OP_DELETE(download_callback);
			return;
		}

		TempBuffer buf;
		page->m_title.Set(frames_doc->Title(&buf));
		page->m_frames_doc = frames_doc;

		if (pos == -1)
		{
			pos = AddLast(page, parent);
		}
		else
		{
			pos = InsertBefore(page, pos);
		}
	}

	// traverse links and add them

	LogicalDocument *logdoc = frames_doc->GetLogicalDocument();
	if (logdoc)
	{
		HTML_Element* h_elm = logdoc->GetFirstHE(HE_A);

		if (page->m_last_element && !redraw)
		{
			while (h_elm)
			{
				if (page->m_last_element == h_elm)
				{
					h_elm = (HTML_Element*) h_elm->Next();
					break;
				}

				h_elm = (HTML_Element*) h_elm->Next();
			}

			if (!h_elm)
			{
				h_elm = logdoc->GetFirstHE(HE_A);
			}
		}

		while (h_elm)
		{
			page->m_last_element = h_elm;

			URL* link_url;
			if (h_elm->Type() == HE_A &&
				(link_url = h_elm->GetA_URL()) != NULL &&
				!(in_sub_doc && link_url->Type() == URL_JAVASCRIPT)) // Don't collect javascript urls in sub frames since they will run in the wrong context (the top document's context)
			{
				OpString url_top;
				h_elm->GetA_URL()->GetAttribute(URL::KUniName_Username_Password_Hidden, url_top, TRUE);
				const uni_char* url_rel = h_elm->GetA_URL()->UniRelName();

				uni_char* full_tag = Window::ComposeLinkInformation(url_top.CStr(), url_rel);

				if(full_tag && *full_tag)
				{
					LinksModelItem* item = NULL;
					BOOL new_item = FALSE;

					m_hash_table.GetData(full_tag, &item);

					if (!item)
					{
						new_item = TRUE;
						ViewActionFlag view_action_flag;
						TransferManagerDownloadCallback * download_callback = OP_NEW(TransferManagerDownloadCallback, (frames_doc->GetDocManager(), *(h_elm->GetA_URL()), NULL, view_action_flag));
						item = OP_NEW(LinksModelItem, (windowcommander, download_callback, full_tag));
						if (!item)
						{
							OP_DELETE(download_callback);
							OP_DELETEA(full_tag);
							continue;
						}
					}

					const uni_char* title = h_elm->GetA_Title();

					BOOL gained_title = FALSE;

					if (!item->m_has_true_title && title)
					{
						item->m_title.Set(title);
						item->m_has_true_title = TRUE;
						gained_title = TRUE;
					}

					// use child elements to build text.. works in most cases, right?

					HTML_Element* child_elm = h_elm->FirstChild();

					BOOL changed = FALSE;

					const uni_char* alt_text = NULL;		

					while (child_elm && child_elm != h_elm)
					{			
						if (child_elm->Type() == HE_IMG && child_elm->GetIMG_Alt())
						{
							alt_text = child_elm->GetIMG_Alt();
						}
						else if (child_elm->Type() == HE_TEXT)
						{
							Head prop_list;
							LayoutProperties* layout = LayoutProperties::CreateCascade(child_elm, prop_list, frames_doc->GetHLDocProfile());

							if (layout)
							{
								if (changed)
								{
									item->m_title.Append(child_elm->Content());
									CleanTextData(item->m_title);

									if (layout->GetProps()->font_weight < item->m_font_weight)
									{
										item->m_font_weight = layout->GetProps()->font_weight;
									}
								}
								else if (layout->GetProps()->font_size > item->m_font_size || (layout->GetProps()->font_size == item->m_font_size && layout->GetProps()->font_weight > item->m_font_weight))
								{
									item->m_init = FALSE;
									item->m_font_size = layout->GetProps()->font_size;
									item->m_font_weight = layout->GetProps()->font_weight;
									item->m_font_color = HTM_Lex::GetColValByIndex(layout->GetProps()->font_color);

									if (!item->m_has_true_title)
									{
										gained_title = item->m_title.IsEmpty();
										item->m_title.Set(child_elm->Content());
										CleanTextData(item->m_title);
									}
									changed = TRUE;
								}
							}

							prop_list.Clear();

							if (!changed || item->m_has_true_title)
							{
								break;
							}
						}

						if (child_elm->FirstChild())
						{
							child_elm = child_elm->FirstChild();
						}
						else
						{
							for(;child_elm != h_elm; child_elm = child_elm->Parent())
							{
								if(child_elm->Suc())
								{
									child_elm = child_elm->Suc();
									break;
								}
							}
						}
					}

					if (item->m_title.IsEmpty() && alt_text)
					{
						item->m_title.Set(alt_text);
						gained_title = TRUE;
						CleanTextData(item->m_title);
					}

					if (new_item)
					{
						/*INT32 index = */AddLast(item, pos);
						m_hash_table.Add(item->m_url.CStr(), item);
					}
					else if (changed && (!redraw || gained_title))
					{
						item->m_init = FALSE;
						item->Change(gained_title);
					}
					// if 0, set it to a default
					if(!item->m_font_color)
					{
						item->m_font_color = OP_RGB(50, 50, 50);
					}
				}
				OP_DELETEA(full_tag);
			}

			h_elm = (HTML_Element*) h_elm->Next();
		}
	}

	// traverse child frames recursivly

	FramesDocElm* elm = frames_doc->GetFrmDocRoot();

	while (elm)
	{
		if (elm->GetDocManager() && elm->GetDocManager()->GetCurrentDoc() && elm->GetDocManager()->GetCurrentDoc()->Type() == DOC_FRAMES)
		{
			UpdateLinks(windowcommander, (FramesDocument*) elm->GetDocManager()->GetCurrentDoc(), redraw, pos, TRUE);
		}

		if (elm->FirstChild())
		{
			elm = elm->FirstChild();
		}
		else
		{
			while (!elm->Suc() && elm->Parent())
			{
				elm = elm->Parent();
			}

			elm = elm->Suc();
		}
	}
}

INT32 LinksModel::GetColumnCount()
{
	return 2;
}

OP_STATUS LinksModel::GetColumnData(ColumnData* column_data)
{
	OpString name, address;
	g_languageManager->GetString(Str::SI_IDSTR_TRANSWIN_NAME_COL, name);
	g_languageManager->GetString(Str::SI_LOCATION_TEXT, address);

	const uni_char* pages_strings[] ={name.CStr(), address.CStr()};

	column_data->text.Set(pages_strings[column_data->column]);

	return OpStatus::OK;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS LinksModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_LINKS, type_string);
}
#endif

