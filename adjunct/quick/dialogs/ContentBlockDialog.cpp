/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SUPPORT_VISUAL_ADBLOCK

#include "ContentBlockDialog.h"

#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpEdit.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "modules/hardcore/keys/opkeys.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/content_filter/content_filter.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"



#ifndef CF_CAP_HAS_CONTENT_BLOCK_FILTER_CREATION

// count the number of cnt_char in string
int CountCharacters(const uni_char *string, uni_char cnt_char, int len)
{
	int count = 0;

	while(string && *string != '\0' && len > 0)
	{
		if(*string == cnt_char)
		{
			count++;
		}
		string++;
		len--;
	}
	return count;
}


ContentBlockFilterCreation::ContentBlockFilterCreation(SimpleTreeModel *initial_content_blocked, const URL url)
{
	m_initial_content_blocked = initial_content_blocked;
	m_url = url;
}
ContentBlockFilterCreation::~ContentBlockFilterCreation()
{
}

OP_STATUS ContentBlockFilterCreation::CreateFilterFromURL(URL& homeurl, const uni_char *url, OpString& result)
{
	if(uni_stristr(url, (const char *)"*"))
	{
		return result.Set(url);
	}

	/*
	** Start of code to create patterned URLs to block
	*/
	// handle flash: http://flash.vg.no/annonser/startour/startour_restplass.swf
	const uni_char *swf = NULL;

	swf = uni_stristr(url, (const char *)"swf");
	if(swf == NULL)
	{
		swf = url + uni_strlen(url);
	}
	if(swf)
	{
		while(swf-- != url)
		{
			// search back to the last slash
			if(*swf == '/')
			{
				swf++;
				break;
			}
		}
		if(swf != url)
		{
			// we should now have http://flash.vg.no/annonser/startour/*
			// let's see if we can shorten it down a bit
			int count = CountCharacters(url, '/', swf - url);
			if(count > 4)
			{
				swf--;
				// too long path, let's shorten it down to 2 levels (after http://)
				while(swf-- != url)
				{
					// search back to the last slash
					if(*swf == '/')
					{
						if(--count == 4)
						{
							swf++;
							break;
						}
					}
				}
			}
			result.Empty();
			if(count < 4)
			{
				if(OpStatus::IsError(result.Append(url)))
				{
					return OpStatus::ERR_NO_MEMORY;
				}

			}
			else
			{
				if(OpStatus::IsError(result.Append(url, swf - url)))
				{
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			BOOL go_on = TRUE;

			while(go_on)
			{
				OpString homeurl_string;
				RETURN_IF_ERROR(homeurl.GetAttribute(URL::KUniName_Username_Password_Hidden, homeurl_string));
				if(result.Compare(homeurl_string.CStr(), result.Length()) == 0)
				{
					BOOL slash_found = FALSE;
					// matches main page url, we can't have that
					while(*swf++)
					{
						// search back to the last slash
						if(*swf == '/')
						{
							swf++;
							result.Empty();
							if(OpStatus::IsError(result.Append(url, swf - url)))
							{
								return OpStatus::ERR_NO_MEMORY;
							}
							slash_found = TRUE;
							break;
						}
					}
					if(!slash_found)
					{
						result.Empty();
						if(OpStatus::IsError(result.Append(url, swf - url)))
						{
							return OpStatus::ERR_NO_MEMORY;
						}
						go_on = FALSE;
					}
				}
				else
				{
					go_on = FALSE;
				}

			}
		}
	}
	if(result[result.Length() - 1] != '*')
	{
		result.Append(UNI_L("*"));
	}
	return OpStatus::OK;
}

OP_STATUS ContentBlockFilterCreation::GetCreatedFilterURLs(SimpleTreeModel& pattern_content_blocked)
{
	if(m_initial_content_blocked)
	{
		UINT32 cnt;

		for(cnt = 0; cnt < (UINT32)m_initial_content_blocked->GetCount(); cnt++)
		{
			const uni_char *url = m_initial_content_blocked->GetItemString(cnt);
			SimpleTreeModelItem *item = m_initial_content_blocked->GetItemByIndex(cnt);
			if(item)
			{
				if(item->GetUserData() == (void *)BLOCKED_EDITED)
				{
					pattern_content_blocked.AddItem(url, NULL, 0, -1, item->GetUserData());
					continue;
				}
			}
			if(uni_strchr(url, '*'))
			{
				pattern_content_blocked.AddItem(url, NULL, 0, -1, item ? item->GetUserData() : (void *)BLOCKED_FILTER);
				continue;
			}
			OpString newurl;
			if(OpStatus::IsError(ContentBlockFilterCreation::CreateFilterFromURL(m_url, url, newurl)))
			{
				pattern_content_blocked.DeleteAll();
				return OpStatus::ERR_NO_MEMORY;
			}
			if(newurl.CStr()[newurl.Length() - 1] == '/')
			{
				newurl.Append(UNI_L("*"));
			}

			BOOL found = FALSE;
			INT32 cnt;

			for(cnt = 0; cnt < pattern_content_blocked.GetCount(); cnt++)
			{
				const uni_char *str = pattern_content_blocked.GetItemString(cnt);

				if(str && newurl.Compare(str) == 0)
				{
					found = TRUE;
					break;
				}
			}
			if(!found)
			{
				pattern_content_blocked.AddItem(newurl.CStr(), NULL, 0, -1, item ? item->GetUserData() : (void *)BLOCKED_FILTER);
			}
		}
	}
	else
	{
		return OpStatus::ERR_NULL_POINTER;
	}
	return OpStatus::OK;
}

#endif // CF_CAP_HAS_CONTENT_BLOCK_FILTER_CREATION

ContentBlockDialog::ContentBlockDialog(BOOL started_from_menu) :
		m_model(NULL),
		m_model_all(NULL),
		m_started_from_menu(started_from_menu)
{
}


ContentBlockDialog::~ContentBlockDialog()
{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
	m_model_all->DeleteAll();
	OP_DELETE(m_model);
	OP_DELETE(m_model_all);
#else
	OP_DELETE(m_model_all);
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
}

#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
void ContentBlockDialog::Init(DesktopWindow *parent, OpVector<ContentFilterItem> *content_blocked)
#else
void ContentBlockDialog::Init(DesktopWindow *parent, ContentBlockTreeModel *content_blocked)
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
	m_model = OP_NEW(ContentBlockTreeModel, ());

	if(m_model)
	{
		UINT32 i;

		for(i = 0; i < content_blocked->GetCount(); i++)
		{
			ContentFilterItem *item = content_blocked->Get(i);

			m_model->AddItem(item->GetURL(), NULL, 0, -1, item);
		}
	}
#else
	m_model = content_blocked;
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL

	if(m_model)
	{
		OpString message;
		g_languageManager->GetString(Str::D_BLOCKED_ON_CURRENT_PAGE, message);

		m_model->SetColumnData(0, message.CStr());
	}
	// temporary just to get the items

	OpVector<OpString> items;
	OP_STATUS s = g_urlfilter->CreateExcludeList(items);
	m_model_all = OP_NEW(ContentBlockTreeModel, ());

	if(OpStatus::IsSuccess(s) && m_model_all)
	{
			UINT32 i;
			// OpString16 tmp;
			for(i = 0; i < items.GetCount(); i++)
			{			   
				m_model_all->AddItem(items.Get(i)->CStr(), NULL, 0, -1, NULL);
			}
		OpString message;
		g_languageManager->GetString(Str::D_ALL_BLOCKED_SITES, message);

		m_model_all->SetColumnData(0, message.CStr());
	}
	Dialog::Init(parent);
}

void ContentBlockDialog::OnInit()
{
	OpTreeView *edit;
	OpQuickFind* quickfind = NULL;

	if(m_started_from_menu)
	{
		OpWidget *widget = GetWidgetByName("Blocked_now");
		if(widget)
		{
			DeletePage(widget);
		}
//		ShowWidget("Blocked_now", FALSE);
		ShowWidget("All_blocked_content", TRUE);

		edit = (OpTreeView *)GetWidgetByName("Block_content_urls_all");
		if(edit)
		{
			edit->SetTreeModel(m_model_all);
			edit->SetSelectedItem(0);
			edit->SetReselectWhenSelectedIsRemoved(TRUE);
			edit->SetDragEnabled(FALSE);

			quickfind = (OpQuickFind*) GetWidgetByName("Quickfind_edit_all");
		}
	}
	else
	{
		ShowWidget("Blocked_now", TRUE);

		OpWidget *widget = GetWidgetByName("All_blocked_content");
		if(widget)
		{
			DeletePage(widget);
		}
//		ShowWidget("All_blocked_content", FALSE);

		edit = (OpTreeView *)GetWidgetByName("Block_content_urls");
		if(edit)
		{
			edit->SetTreeModel(m_model);
			edit->SetSelectedItem(0);
			edit->SetReselectWhenSelectedIsRemoved(TRUE);
			edit->SetDragEnabled(FALSE);
			
			quickfind = (OpQuickFind*) GetWidgetByName("Quickfind_edit");
		}
	}
	if(quickfind && edit)
	{
		edit->SetMatchType(MATCH_ALL);
		edit->SetAutoMatch(TRUE);

		quickfind->SetTarget(edit);
//		quickfind->SetFocus(FOCUS_REASON_OTHER);
	}
	Dialog::OnInit();
}

void ContentBlockDialog::OnItemEditAborted(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	OpTreeView *edit = (OpTreeView *)GetWidgetByName("Block_content_urls");

	if(edit != widget)
	{
		edit = (OpTreeView *)GetWidgetByName("Block_content_urls_all");
	}
	if(edit == widget)
	{
		OpTreeModelItem* item = edit->GetItemByPosition(pos);
		if(item)
		{
			INT32 model_pos = edit->GetModelPos(pos);
			if(model_pos > -1)
			{
				SimpleTreeModel *model = static_cast<SimpleTreeModel *>(edit->GetTreeModel());
				if(model)
				{
					FilterURLnode node;
					OpString oldurl;

					oldurl.Set(model->GetItemString(model_pos));

					node.SetURL(text.CStr());
					node.SetIsExclude(TRUE);

					g_urlfilter->DeleteURL(&node);

					if(!text.CompareI(UNI_L("http://")))
					{
						model->Delete(model_pos);
					}
					else if(!text.HasContent() && !oldurl.CompareI(UNI_L("http://")))
					{
						model->Delete(model_pos);
					}
					else if(oldurl.Compare(text))
					{
						model->SetItemData(model_pos, 0, oldurl.CStr());
					}
				}
			}
		}
	}
}

void ContentBlockDialog::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	OpTreeView *edit = (OpTreeView *)GetWidgetByName("Block_content_urls");

	if(edit != widget)
	{
		edit = (OpTreeView *)GetWidgetByName("Block_content_urls_all");
	}
	if(edit == widget)
	{
		OpTreeModelItem* item = edit->GetItemByPosition(pos);
		if(item)
		{
			INT32 model_pos = edit->GetModelPos(pos);
			if(model_pos > -1)
			{
				// Fix for just doing Add and Enter, or Add, clear text, then enter
				if(!text.HasContent() || !text.CompareI(UNI_L("http://")))
				{
					SimpleTreeModel *model = static_cast<SimpleTreeModel *>(edit->GetTreeModel());
					if(model)
					{
						model->Delete(model_pos);
						return;
					}
				}
				// Fix the URL for conformance
				if(text.CompareI(UNI_L("http://"), 7) && text.CompareI(UNI_L("ftp://"), 6) && text.CompareI(UNI_L("https://"), 8))
				{
					// don't add anything if it starts with * and is longer than 1 char
					if(text.Compare(UNI_L("*"), 1) && text.Length() > 1)
					{
						// missing http://
						text.Insert(0, UNI_L("http://"));
					}
				}
				if(text[text.Length() - 1] != '*')
				{
					if(CountCharacters(text.CStr(), '/', text.Length()) == 2)
					{
						if(text[text.Length() - 1] != '/')
						{
							// no slash or * at the end, add it
							text.Append(UNI_L("/*"));
						}
					}
				}
				if(m_started_from_menu || GetCurrentPage() == (ContentBlockPage)PageAll)
				{
					OpString oldurl;

					oldurl.Set(m_model_all->GetItemString(model_pos));

					if(oldurl.Compare(UNI_L("http://")))
					{
						// don't send a delete for the default http:// entry in the list as it doesn't exist in the content_filter module
						FilterURLnode node;

						node.SetURL(oldurl.CStr());
						node.SetIsExclude(TRUE);

						g_urlfilter->DeleteURL(&node);
					}
					m_model_all->SetItemData(model_pos, 0, text.CStr());
					SimpleTreeModelItem *item = m_model_all->GetItemByIndex(model_pos);
					if(item)
					{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
						ContentFilterItem *cfitem = static_cast<ContentFilterItem *>(item->GetUserData());
						if(cfitem)
						{
							cfitem->SetContentBlockedType(BLOCKED_EDITED);	// edited flag
						}
#else
						item->SetUserData((void *)BLOCKED_EDITED);	// edited flag
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
					}
					g_urlfilter->AddURLString(text.CStr(), TRUE, NULL);
				}
				else
				{
					m_model->SetItemData(model_pos, 0, text.CStr());
					SimpleTreeModelItem *item = m_model->GetItemByIndex(model_pos);
					if(item)
					{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
						ContentFilterItem *cfitem = static_cast<ContentFilterItem *>(item->GetUserData());
						if(cfitem)
						{
							cfitem->SetContentBlockedType(BLOCKED_EDITED);	// edited flag
						}
#else
						item->SetUserData((void *)BLOCKED_EDITED);	// edited flag
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
					}
				}
			}
		}
	}
}

void ContentBlockDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if(widget->IsNamed("Block_content_delete") || widget->IsNamed("Block_content_delete_all"))
	{
		OpTreeView *edit = (OpTreeView *)GetWidgetByName("Block_content_urls");
		if(widget->IsNamed("Block_content_delete_all"))
		{
			edit = (OpTreeView *)GetWidgetByName("Block_content_urls_all");
		}
		if(edit)
		{
			INT32 pos = edit->GetSelectedItemModelPos();
			if(pos > -1)
			{
				if(m_started_from_menu || GetCurrentPage() == (ContentBlockPage)PageAll)
				{
					const uni_char *str = m_model_all->GetItemString(pos);
					FilterURLnode node;

					node.SetURL(str);
					node.SetIsExclude(TRUE);

					g_urlfilter->DeleteURL(&node);
					m_model_all->Delete(pos);

				}
				else
				{
					SimpleTreeModelItem *item = m_model->GetItemByIndex(pos);
					if((item && item->GetUserData() == (void *)BLOCKED_PREVIOUSLY)
						|| item->GetUserData() == (void *)BLOCKED_EDITED)
					{
						const uni_char *str = m_model->GetItemString(pos);
						FilterURLnode node;

						node.SetURL(str);
						node.SetIsExclude(TRUE);

						g_urlfilter->DeleteURL(&node);
					}
					m_model->Delete(pos);
				}
			}
		}
	}
	else if(widget->IsNamed("Block_content_edit") || widget->IsNamed("Block_content_edit_all"))
	{
		OpTreeView *edit = (OpTreeView *)GetWidgetByName("Block_content_urls");
		if(widget->IsNamed("Block_content_edit_all"))
		{
			edit = (OpTreeView *)GetWidgetByName("Block_content_urls_all");
		}
		if(edit)
		{
			INT32 tree_post = edit->GetSelectedItemPos();
			if(tree_post > -1)
			{
				edit->EditItem(tree_post, 0, TRUE);
			}
		}
	}
	else if(widget->IsNamed("Block_content_new") || widget->IsNamed("Block_content_new_all"))
	{
		OpQuickFind *find = (OpQuickFind *)GetWidgetByName("Quickfind_edit");
		OpTreeView *edit = (OpTreeView *)GetWidgetByName("Block_content_urls");
		if(widget->IsNamed("Block_content_new_all"))
		{
			edit = (OpTreeView *)GetWidgetByName("Block_content_urls_all");
			find = (OpQuickFind *)GetWidgetByName("Quickfind_edit_all");
		}
		if(edit)
		{
			INT32 pos = 0;

			if(find)
			{
				find->SetText(UNI_L(""));
			}
			if(m_started_from_menu || GetCurrentPage() == (ContentBlockPage)PageAll)
			{
				pos = m_model_all->AddItem(UNI_L("http://"));
			}
			else
			{
				pos = m_model->AddItem(UNI_L("http://"));
			}
			edit->EditItem(pos, 0, TRUE);
		}
	}
}

void ContentBlockDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	OpTreeView *edit = (OpTreeView *)GetWidgetByName("Block_content_urls");

	if(edit != widget)
	{
		edit = (OpTreeView *)GetWidgetByName("Block_content_urls_all");
	}
	if (widget == edit)
	{
		OpTreeModelItem* item = edit->GetItemByPosition(pos);
		if(item)
		{
			if(nclicks == 2)
			{
				INT32 model_pos = edit->GetModelPos(pos);
				if(model_pos > -1)
				{
					edit->EditItem(model_pos, 0, TRUE);
				}
			}
		}
		OpWidget *edit_button = GetWidgetByName("Block_content_edit");
		if(!edit_button)
		{
			edit_button = GetWidgetByName("Block_content_edit_all");
		}
		OpWidget *delete_button = GetWidgetByName("Block_content_delete");
		if(!delete_button)
		{
			delete_button = GetWidgetByName("Block_content_delete_all");
		}
		if(edit_button)
		{
			edit_button->SetEnabled(item != NULL);
		}
		if(delete_button)
		{
			delete_button->SetEnabled(item != NULL);
		}
	}
};

void ContentBlockDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;
	switch (index)
	{
		case 0:
		{
			action = GetOkAction();
			g_languageManager->GetString(Str::DI_IDBTN_CLOSE, text);
			is_default = TRUE;
			name.Set(WIDGET_NAME_BUTTON_CLOSE);
			break;
		}
	}
}

void ContentBlockDialog::OnCancel()
{
}

UINT32 ContentBlockDialog::OnOk()
{
	if(m_started_from_menu)
	{
		g_urlfilter->WriteL();
	}
	return 0;
}

#endif // SUPPORT_VISUAL_ADBLOCK

