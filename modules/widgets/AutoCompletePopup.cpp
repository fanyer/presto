/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/AutoCompletePopup.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/display/vis_dev.h"
#include "modules/display/fontdb.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/forms/formsuggestion.h"
#include "modules/forms/piforms.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"

#include "modules/history/OpHistoryModel.h"

#include "modules/widgets/OpEdit.h"

#include "modules/pi/OpPainter.h"
#include "modules/pi/OpWindow.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/display/VisDevListeners.h"

#include "modules/widgets/OpListBox.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/windowcommander/src/WindowCommander.h"
#if defined (QUICK)
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/Application.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#endif

#ifdef M2_SUPPORT
#include "adjunct/m2/src/engine/engine.h"
#endif

// == AutoCompleteWindow =============================================

AutoCompleteWindow::AutoCompleteWindow(AutoCompletePopup* autocomp)
	: autocomp(autocomp)
{
}

AutoCompleteWindow::~AutoCompleteWindow()
{
	g_widget_globals->curr_autocompletion = NULL;
	g_widget_globals->autocomp_popups_visible = FALSE;
	autocomp->window = NULL;
	autocomp->listbox = NULL;
}

// == AutoCompletePopup =============================================

class AutoCompleteListboxListener : public OpWidgetListener
{
public:
	AutoCompleteListboxListener(AutoCompletePopup* autocomp)
		: autocomp(autocomp)
	{
	}

	void OnChange(OpWidget *widget, BOOL changed_by_mouse)
	{
		autocomp->UpdateEdit();
	}

#ifndef MOUSELESS
	void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
	{
		if (down && nclicks == 1)
		{
			autocomp->current_item = pos;
			autocomp->UpdateEdit();
			autocomp->CommitLastSelectedValue();
			autocomp->SelectAndClose();
		}
	}
#endif // !MOUSELESS

private:
	AutoCompletePopup* autocomp;
};

// == AutoCompletePopup =============================================

AutoCompletePopup::AutoCompletePopup(OpWidget* edit)
	: edit(edit)
	, type(AUTOCOMPLETION_OFF)
	, current_item(-1)
	, num_items(0)
	, num_columns(1)
	, items(NULL)
	, opened_manually(FALSE)
	, listener(NULL)
	, listbox(NULL)
	, window(NULL)
#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
	, m_google_timer(NULL)
#endif
	, change_field_while_browsing_alternatives(TRUE)
	, target(NULL)
{
}

AutoCompletePopup::~AutoCompletePopup()
{
#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
	g_main_message_handler->UnsetCallBacks(this);
#endif
	ClosePopup(TRUE);
	FreeItems();
	if (this == g_widget_globals->curr_autocompletion)
		g_widget_globals->curr_autocompletion = NULL;
#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
	OP_DELETE(m_google_timer);
#endif
}

void AutoCompletePopup::SetType(AUTOCOMPL_TYPE type)
{
	this->type = type;
#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
	OP_DELETE(m_google_timer);
	m_google_timer = NULL;
#endif
}

void AutoCompletePopup::SetEditTextWithCaretAtEnd(const uni_char* new_text)
{
	if (edit->IsOfType(OpTypedObject::WIDGET_TYPE_EDIT))
	{
		OpEdit *edit = (OpEdit *) this->edit;
		OpStatus::Ignore(edit->SetText(UNI_L("")));
		OpStatus::Ignore(edit->InsertText(new_text));
	}
	else
		OpStatus::Ignore(edit->SetText(new_text));
}

void AutoCompletePopup::SendEditOnChange(BOOL changed_by_mouse/* = FALSE*/)
{
	if (OpWidgetListener* edit_listener = edit->GetListener())
	{
		OpString edit_text;
		OP_STATUS status = edit->GetText(edit_text);
		if (OpStatus::IsSuccess(status))
		{
			if (edit_text.Compare(m_last_edit_text))
			{
				edit_listener->OnChange(edit, changed_by_mouse);
				m_last_edit_text.TakeOver(edit_text);
			}
		}
		else if (OpStatus::IsMemoryError(status))
			edit->ReportOOM();
	}
}

void AutoCompletePopup::UpdateEdit()
{
	if (type == AUTOCOMPLETION_GOOGLE)
		return;

	if (current_item == -1)
	{
		SetEditTextWithCaretAtEnd(user_string.CStr());
	}
	else if (num_items)
	{
		int tmpidx = current_item * num_columns;

		OpString string1, string2;

		if (string1.Set(items[tmpidx]) != OpStatus::OK)
		{
			edit->ReportOOM();
			return;
		}

		if (num_columns > 1)
		{
			if (string2.Set(items[tmpidx+1]) != OpStatus::OK)
			{
				edit->ReportOOM();
				return;
			}
		}

		const uni_char *sep = UNI_L(",");

		if (type == AUTOCOMPLETION_ACTIONS)
		{
			sep = UNI_L("|&+>");
		}
		else if (type == AUTOCOMPLETION_HISTORY)
		{
			sep = UNI_L(""); // we don't autocomplete against comma-separated URLs
		}

		int next_address  = KNotFound;

		while (*sep)
		{
			int next = user_string.FindLastOf(sep[0]);

			if (next_address == KNotFound || (next != KNotFound && next > next_address))
			{
				next_address = next;
			}
			sep++;
		}

		if (next_address == KNotFound)
		{
			next_address = 0;
		}
		else
		{
			next_address++;
			if (user_string[next_address] == ' ' )
			{
				next_address++;
			}
		}
		OpString string;

		if (OpStatus::IsError(string.Append(user_string.CStr(),next_address)))
		{
			edit->ReportOOM();
			return;
		}

		if (OpStatus::IsError(string.Append(string1)))
		{
			edit->ReportOOM();
			return;
		}

		if (type == AUTOCOMPLETION_CONTACTS)
		{
			if (OpStatus::IsError(string.Append(string2)))
			{
				edit->ReportOOM();
				return;
			}
		}

		if (change_field_while_browsing_alternatives)
		{
			// Else it's done when the popup closes
			SetEditTextWithCaretAtEnd(string.CStr());
		}
		else
		{
			if (OpStatus::IsError(last_selected_string.Set(string)))
			{
				edit->ReportOOM();
			}
		}
	}
	edit->ScrollIfNeeded(); // Good or bad. What does the user want?
}

BOOL AutoCompletePopup::SelectAndClose()
{
	if (window)
	{
		BOOL selected_item = current_item != -1;
#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
		if (current_item != -1 && type == AUTOCOMPLETION_GOOGLE)
		{
			ClosePopup();

			if (items && current_item < num_items)
			{
				g_application->GoToPage(items[current_item * 2 + 1]);
			}
		}
		else
#endif
		{
			ClosePopup();

			if (edit->GetAction())
			{
				OpString tmp;
				if (OpStatus::IsSuccess(edit->GetText(tmp)))
				{
					edit->GetAction()->SetActionDataString(tmp.CStr());
					g_input_manager->InvokeAction(edit->GetAction(), edit->GetParentInputContext());
				}
			}
			else
				SendEditOnChange(FALSE);
		}
		return selected_item;
	}
	return FALSE;
}

void AutoCompletePopup::FreeItems()
{
	for(int i = 0; i < num_items * num_columns; i++)
		OP_DELETEA(items[i]);
	OP_DELETEA(items);
	items = NULL;
	num_items = 0;
}

void AutoCompletePopup::OpenManually()
{
	OpInputAction action(OpInputAction::ACTION_UNKNOWN);
	EditAction(&action, TRUE); // To open the window if suitable
}

BOOL AutoCompletePopup::EditAction(OpInputAction* action, BOOL changed)
{
	if (type == AUTOCOMPLETION_OFF)
		return FALSE;

	opened_manually = action->GetAction() == OpInputAction::ACTION_UNKNOWN;

	FormObject* form_object = edit->GetFormObject(TRUE);
	if (form_object)
	{
		// don't stop dropdowns like Google Suggest from working
		HTML_Element* he = form_object->GetHTML_Element();
		if (he->GetAutocompleteOff())
			return FALSE;
	}
	// no google autocompletion unless we have a clear contract
	if (type == AUTOCOMPLETION_GOOGLE)
		return FALSE;

	if (changed)
	{
		FreeItems();

#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
		StopGoogle();
#endif

		// Get matching items
		OpString tmpstr;
		OpStatus::Ignore(edit->GetText(tmpstr));
		const uni_char* matchString = tmpstr.CStr();
		BOOL show_autocomplete = type == AUTOCOMPLETION_ACTIONS || type == AUTOCOMPLETION_FORM || *matchString;
		if (show_autocomplete)
		{
			switch (type)
			{
				case AUTOCOMPLETION_PERSONAL_INFO:
				{
					OP_STATUS ret_val = g_forms_history->GetItems(matchString, &items, &num_items);
					if (OpStatus::IsError(ret_val))
					{
						edit->ReportOOM();
						return FALSE;
					}
					num_columns = 1;
					break;
				}
#ifdef HISTORY_SUPPORT
				case AUTOCOMPLETION_HISTORY:
				{
					if(globalHistory)
						globalHistory->GetItems(matchString, &items, &num_items); // FIXME:OOM
					num_columns = 3;
# if defined (SUPPORTS_ADD_SEARCH_TO_HISTORY_LIST)
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
					g_searchEngineManager->AddSearchStringToList(matchString, items, num_items);
#endif // DESKTOP_UTIL_SEARCH_ENGINES
# endif
					break;
				}
#endif // HISTORY_SUPPORT
				case AUTOCOMPLETION_CONTACTS:
				{
					const uni_char* next_address = uni_strrchr(matchString, ',');
					if (!next_address)
					{
						next_address = matchString;
					}
					else
					{
						next_address++;
						if (next_address[0] == ' ')
						{
							next_address++;
						}
						if (next_address[0] == 0 )
						{
							num_items = 0;
							break;
						}
					}
					OpString tmp;
					if (OpStatus::IsError(tmp.Set(next_address)))
					{
						edit->ReportOOM();
						return FALSE;
					}

#if defined (QUICK)
					g_hotlist_manager->GetContactNameList( tmp, &items, &num_items);
#endif
					num_columns = 2;
					break;
				}
				case AUTOCOMPLETION_FORM:
				{
					OP_ASSERT(form_object); // This is only supported in web documents
					FormSuggestion form_suggestion(
						form_object->GetDocument(),
						form_object->GetHTML_Element(),
						FormSuggestion::AUTOMATIC);
					BOOL contains_private_data = TRUE; // Play it safe
					OP_STATUS ret_val = form_suggestion.GetItems(opened_manually ? UNI_L("") : matchString, &items, &num_items, &num_columns, &contains_private_data);
					if (OpStatus::IsError(ret_val))
					{
						edit->ReportOOM();
						return FALSE;
					}
					change_field_while_browsing_alternatives = !contains_private_data;
					break;
				}
				case AUTOCOMPLETION_ACTIONS:
				{
					num_columns = 1;

					const uni_char* next_address = uni_strrchr(matchString, '|');

					if (next_address)
					{
						matchString = ++next_address;
					}

					next_address = uni_strrchr(matchString, '&');

					if (next_address)
					{
						matchString = ++next_address;
					}

					next_address = uni_strrchr(matchString, '+');

					if (next_address)
					{
						matchString = ++next_address;
					}

					next_address = uni_strrchr(matchString, '>');

					if (next_address)
					{
						matchString = ++next_address;
					}

					while (uni_isspace(*matchString)) matchString++;

					// match..

					OpVector<uni_char> list;

					for (INT32 actionnum = OpInputAction::ACTION_UNKNOWN + 1; actionnum < OpInputAction::LAST_ACTION; actionnum++)
					{
						OpString action_string_tmp;
						action_string_tmp.Set(OpInputAction::GetStringFromAction(static_cast<OpInputAction::Action>(actionnum)));
						const uni_char* action_string = action_string_tmp.CStr();

						if (!*matchString || uni_stristr(action_string, matchString))
						{
							int new_len = uni_strlen(action_string) + 1;
							uni_char* action_string_copy = OP_NEWA(uni_char, new_len);
							if (!action_string_copy)
							{
								edit->ReportOOM();
								return FALSE;
							}

							uni_strcpy(action_string_copy, action_string);
							if (OpStatus::IsError(list.Add(action_string_copy)))
							{
								edit->ReportOOM();
								return FALSE;
							}
						}
					}

					num_items = list.GetCount();

					if (num_items)
					{
						items = OP_NEWA(uni_char*, num_items);
						if (!items)
						{
							edit->ReportOOM();
							return FALSE;
						}
						for (INT32 i = 0; i < num_items; i++)
						{
							items[i] = list.Get(i);
						}
					}

					break;
				}
#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
				case AUTOCOMPLETION_GOOGLE:
				{
					num_columns = 2;

					if (!m_google_timer)
					{
						m_google_timer = OP_NEW(OpTimer, ());
						if (!m_google_timer)
						{
							edit->ReportOOM();
							break;
						}

						m_google_timer->SetTimerListener(this);
					}

					m_google_timer->Start(500);
					break;
				}
#endif // WIDGETS_AUTOCOMPLETION_GOOGLE

				case AUTOCOMPLETION_M2_MESSAGES:
				{
#ifdef M2_SUPPORT
					if (!g_m2_engine)
						break;

#ifdef SEARCH_ENGINE_FOR_MAIL
					num_items = 1024; //Maximum number of results
					items = OP_NEWA(uni_char*, num_items);
					if (items)
					{
						if (g_m2_engine->GetLexiconWords(items, matchString, num_items) != OpStatus::OK)
						{
							OP_DELETEA(items);
							items = NULL;
							num_items = 0;
						}
					}
#else
					INT32 index;

					g_m2_engine->GetLexiconMatch(matchString, index, num_items);

					if (num_items)
					{
						items = OP_NEWA(uni_char*, num_items);
						if (!items)
						{
							edit->ReportOOM();
							break;
						}
						OpString string;

						for (INT32 i = 0; i < num_items; i++)
						{
							g_m2_engine->GetLexiconWord(index + i, string);
							int new_len = uni_strlen(string.CStr()) + 1;
							uni_char* string_copy = OP_NEWA(uni_char, new_len);
							uni_strcpy(string_copy, string.CStr());
							items[i] = string_copy;
						}
					}
#endif //SEARCH_ENGINE

#endif //M2_SUPPORT
					break;
				}

			}
		}

		UpdatePopup();
	}
	else if ((action->GetAction() == OpInputAction::ACTION_NEXT_LINE || action->GetAction() == OpInputAction::ACTION_PREVIOUS_LINE) && items)
	{
		BOOL send_edit_on_change = FALSE;
		if (listbox)
		{
			const int prev_selected = listbox->GetSelectedItem();
			current_item += action->GetAction() == OpInputAction::ACTION_NEXT_LINE ? 1 : -1;
			listbox->SelectItem(listbox->ih.focused_item, FALSE);

			if (current_item == -2) // jump to last item
			{
				current_item = listbox->CountItems() - 1;
				listbox->ih.focused_item = current_item;
				listbox->SelectItem(listbox->ih.focused_item, TRUE);
			}
			else if (current_item == listbox->CountItems()) // jump to item -1 (user string)
			{
				current_item = - 1;
				listbox->ih.focused_item = 0;
			}
			else // jump to current item
			{
				listbox->ih.focused_item = current_item;
				listbox->SelectItem(listbox->ih.focused_item, TRUE);
			}
			listbox->ScrollIfNeeded();
			listbox->Invalidate(listbox->GetBounds());

			// for form ac:s change events should be sent whenever the
			// value of the input changes. this doesn't fly for ui
			// widgets (because of eg auto-complete address bar).
			if (type == AUTOCOMPLETION_FORM)
			{
				const int selected = listbox->GetSelectedItem();
				if (selected >= 0 && selected != prev_selected)
					send_edit_on_change = TRUE;
			}
		}

		UpdateEdit();
		if (send_edit_on_change)
			SendEditOnChange(FALSE);
	}
	else if( action->GetAction() == OpInputAction::ACTION_PAGE_UP || action->GetAction() == OpInputAction::ACTION_PAGE_DOWN )
	{
		if (listbox)
		{
			INT32 step = listbox->GetBounds().height / listbox->ih.highest_item;
			current_item += action->GetAction() == OpInputAction::ACTION_PAGE_DOWN ? step : -step;

			listbox->SelectItem(listbox->ih.focused_item, FALSE);

			if (current_item < -1 )
			{
				current_item = -1;
				listbox->ih.focused_item = current_item;
				listbox->SelectItem(listbox->ih.focused_item, TRUE);
			}
			else if (current_item >= listbox->CountItems())
			{
				current_item = listbox->CountItems()-1;
				listbox->ih.focused_item = listbox->CountItems()-1;
				listbox->SelectItem(listbox->ih.focused_item, TRUE);
			}
			else
			{
				listbox->ih.focused_item = current_item;
				listbox->SelectItem(listbox->ih.focused_item, TRUE);
			}
			listbox->ScrollIfNeeded();
			listbox->Invalidate(listbox->GetBounds());
		}

		UpdateEdit();
	}
	else if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED && action->GetActionData() == OP_KEY_ENTER)
	{
#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE
		StopGoogle();
#endif
		if (window)
			return SelectAndClose();
	}

	return FALSE;
}

void AutoCompletePopup::UpdatePopup()
{
#if defined (_DIRECT_URL_WINDOW_)
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AutoDropDown) == FALSE)
		return; // User has disabled the autocompletion
#endif

	if (num_items == 0)
	{
		ClosePopup();
		return;
	}
	if (!window)
	{
		if (OpStatus::IsError(CreatePopup()))
		{
			edit->ReportOOM();
			return;
		}
	}

	OP_STATUS status = edit->GetText(user_string);
	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
		{
			edit->ReportOOM();
		}
		return;
	}

	// Update the listbox inside the popup

	listbox->RemoveAll();
	current_item = -1;

	for(int i = 0; i < num_items; i++)
	{
		OP_STATUS status;
		if (num_columns == 1)
			status = listbox->AddItem(items[i]);
		else
			status = listbox->AddItem(items[i * num_columns], items[i * num_columns + 1]);

		if (OpStatus::IsMemoryError(status))
		{
			edit->ReportOOM();
			return;
		}

		if (OpStatus::IsSuccess(status) && opened_manually)
		{
			// If it's opened manually (ie not by typing text) we will select first item
			// that match the current string to make it easy to the user to switch to something close to that.
			OpStringItem *item = listbox->ih.GetItemAtIndex(i);
			if (!user_string.IsEmpty() && current_item == -1 && uni_strcmp(item->string.Get(), user_string.CStr()) == 0)
				current_item = i;
		}
	}

	UpdatePopupLookAndPlacement();

	if (current_item != -1)
		listbox->SelectItem(current_item, TRUE);
	g_widget_globals->curr_autocompletion = this;
	g_widget_globals->autocomp_popups_visible = TRUE;
}

void AutoCompletePopup::UpdatePopupLookAndPlacement(BOOL only_if_font_change)
{
	if (!window || !listbox)
		return;

	// Update color and font
	listbox->SetColor(edit->GetColor());

	WIDGET_FONT_INFO wfi = edit->font_info;
	int zoom = edit->GetVisualDevice()->GetScale();
	wfi.size = wfi.size * zoom / 100;
	BOOL changed_font = listbox->SetFontInfo(wfi);

	// Update item sizes
	if (changed_font)
		listbox->RecalculateWidestItem();
	else if (only_if_font_change)
		return;

	// Calculate size

	double fscale = edit->GetVisualDevice()->GetScale() / 100.0;

	INT32 edit_width = (INT32) ((edit->GetRect().width) * fscale) ;
	INT32 width = 0, height = 0;

	if (target)
	{
		// We have a target widget, use that width for the popup width
		edit_width = (INT32) ((target->GetRect().width) * fscale) - 2;
	}

	// Allow autocompletion lists to contain fewer than 10 rows.
	listbox->GetPreferedSize(&width, &height, 0, MIN(10, num_items));

	if (edit_width > width)
		width = edit_width;

	OpRect rect = AutoCompleteWindow::GetBestDropdownPosition(target ? target : edit, width, height);
	window->Show(TRUE, &rect);
	listbox->SetSize( rect.width, rect.height );
	listbox->UpdateScrollbar();
	listbox->InvalidateAll();
}

OP_STATUS AutoCompletePopup::CreatePopup()
{
	// Creating new popup closes and destroys previous one, therefore we
	// shouldn't allow creating popup within a popup.
	OpWindow* parent_window = edit->GetParentOpWindow();

	if (parent_window && parent_window->GetRootWindow()
		&& parent_window->GetRootWindow()->GetStyle() == OpWindow::STYLE_POPUP)
		return OpStatus::ERR;

	// Show popup
	window = OP_NEW(AutoCompleteWindow, (this));
	if (!window)
		return OpStatus::ERR_NO_MEMORY;


	if (OpStatus::IsError(window->Init(OpWindow::STYLE_POPUP, parent_window)))
	{
		OP_DELETE(window);
		window = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	listener = OP_NEW(AutoCompleteListboxListener, (this));
	if (!listener)
	{
		OP_DELETE(window);
		window = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = OpListBox::Construct(&listbox, FALSE, OpListBox::BORDER_STYLE_POPUP);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(listener);
		listener = NULL;
		OP_DELETE(window);
		window = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	WidgetContainer* container = window->GetWidgetContainer();
	container->SetEraseBg(TRUE);

	INT32 left, top, right, bottom;
	listbox->GetInfo()->GetBorders(listbox, left, top, right, bottom);
	if (left || top || right || bottom)
		container->GetRoot()->SetBackgroundColor(OP_RGB(0, 0, 0));
	else
		container->GetRoot()->SetBackgroundColor(OP_RGB(255, 255, 255));
	listbox->SetRect(OpRect(0, 0, 0, 0));

	listbox->SetListener(listener);
	listbox->hot_track = TRUE;
	container->GetRoot()->AddChild(listbox);

	UpdatePopupLookAndPlacement();

	return OpStatus::OK;
}

void AutoCompletePopup::ClosePopup(BOOL immediately)
{
	if (listener)
	{
		if (listbox)
			listbox->SetListener(NULL);

		OP_DELETE(listener);
		listener = NULL;
	}

	if (window)
	{
		window->Close(immediately);
	}
}

void AutoCompletePopup::CommitLastSelectedValue()
{
	if (!change_field_while_browsing_alternatives && !last_selected_string.IsEmpty())
	{
		// else it's set directly before closing the popup

		// this is the time to set the value
		SetEditTextWithCaretAtEnd(last_selected_string.CStr());
	}
}

/*static */BOOL AutoCompletePopup::IsAutoCompletionVisible()
{
	return g_widget_globals->autocomp_popups_visible;
}

/*static */BOOL AutoCompletePopup::IsAutoCompletionHighlighted()
{
	AutoCompletePopup* ac = g_widget_globals->curr_autocompletion;
	if (ac && IsAutoCompletionVisible())
	{
		if (ac->current_item != -1)
			return TRUE;
	}
	return FALSE;
}

/*static */void AutoCompletePopup::CloseAnyVisiblePopup()
{
	AutoCompletePopup* ac = g_widget_globals->curr_autocompletion;
	if (ac)
	{
		ac->CommitLastSelectedValue();
		ac->ClosePopup();

		ac->SendEditOnChange(FALSE);
	}
}

#ifdef WIDGETS_AUTOCOMPLETION_GOOGLE

void AutoCompletePopup::OnTimeOut(OpTimer* timer)
{
	OpString tmpstring;
	RETURN_VOID_IF_ERROR(edit->GetText(tmpstring));
	const uni_char* matchString = tmpstring.CStr();

	if (matchString && *matchString)
	{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		g_searchEngineManager->PrepareSearch(m_google_url, matchString, FALSE, 0);
#endif // DESKTOP_UTIL_SEARCH_ENGINES

		m_google_url.SetAttribute(URL::KDisableProcessCookies, TRUE);
		m_google_url.Load(g_main_message_handler, URL(), TRUE);

		g_main_message_handler->SetCallBack(this, MSG_URL_DATA_LOADED, m_google_url.Id());
	}
}

void AutoCompletePopup::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (m_google_url.GetAttribute(URL::KLoadStatus) != URL_LOADING)
	{
		g_main_message_handler->UnsetCallBacks(this);

		FreeItems();

		URL_DataDescriptor* url_data_desc = m_google_url.GetDescriptor(g_main_message_handler, TRUE);

		if (url_data_desc)
		{
			OpVector<uni_char> list;
			OpString data;

			BOOL more = TRUE;
			UINT32 buf_len = 0;

			while (more && (buf_len = url_data_desc->RetrieveData(more)))
			{
				uni_char* data_buf = (uni_char*)url_data_desc->GetBuffer();
				if (OpStatus::IsError(data.Append(data_buf)))
				{
					edit->ReportOOM();
					return;
				}

				url_data_desc->ConsumeData(buf_len);
			}

			OP_DELETE(url_data_desc);

			const uni_char* buffer = data.CStr();
			const uni_char* prefix = UNI_L("<p class=g><a href=");

			while (buffer)
			{
				buffer = uni_strstr(buffer, prefix);

				if (buffer)
				{
					buffer += uni_strlen(prefix);

					const uni_char* end = uni_strchr(buffer, '>');

					if (!end)
						break;

					INT32 len = end - buffer;

					uni_char* copy = OP_NEWA(uni_char, len + 1);
					if (copy == NULL)
					{
						edit->ReportOOM();
						return;
					}

					uni_strncpy(copy, buffer, len);
					copy[len] = 0;
					if (OpStatus::IsError(list.Add(copy)))
					{
						edit->ReportOOM();
						OP_DELETEA(copy);
						return;
					}


					buffer = end + 1;

					end = uni_strstr(buffer, UNI_L("</a>"));

					if (!end)
						break;

					len = end - buffer;

					OpString title;

					if (OpStatus::IsError(title.Set(buffer, len)))
					{
						edit->ReportOOM();
						return;
					}
					INT32 index;

					while ((index = title.Find(UNI_L("<b>"))) != KNotFound)
					{
						title.Delete(index, 3);
					}

					while ((index = title.Find(UNI_L("</b>"))) != KNotFound)
					{
						title.Delete(index, 4);
					}

					if (OpStatus::IsError(title.Append(UNI_L("     "))))
					{
						edit->ReportOOM();
						OP_DELETEA(copy);
						return;
					}
					len = title.Length();

					copy = OP_NEWA(uni_char, len + 1);
					if (copy == NULL)
					{
						edit->ReportOOM();
						return;
					}
					uni_strncpy(copy, title.CStr(), len);
					copy[len] = 0;
					if (OpStatus::IsError(list.Insert(list.GetCount() - 1, copy)))
					{
						edit->ReportOOM();
						OP_DELETEA(copy);
						return;
					}
				}
			}

			INT32 count = list.GetCount();
			num_items = count / 2;

			if (num_items)
			{
				items = OP_NEWA(uni_char*, count);
				if (items == NULL)
				{
					edit->ReportOOM();
					return;
				}
				for (INT32 i = 0; i < count; i++)
				{
					items[i] = list.Get(i);
				}
			}
		}

		UpdatePopup();
	}
}

void AutoCompletePopup::StopGoogle()
{
	m_google_url.StopLoading(g_main_message_handler);
	g_main_message_handler->UnsetCallBacks(this);

	if (m_google_timer)
	{
		m_google_timer->Stop();
	}
}

#endif // WIDGETS_AUTOCOMPLETION_GOOGLE
