// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2004 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
#include "core/pch.h"

#include "ContactsPanel.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/ContactPropertiesDialog.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"

#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"

#include "modules/display/vis_dev.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/widgets/WidgetContainer.h"

/***********************************************************************************
**
**	ContactsPanel
**
***********************************************************************************/

ContactsPanel::ContactsPanel()
{
}

// ----------------------------------------------------

OP_STATUS ContactsPanel::Init()
{
	RETURN_IF_ERROR(OpHotlistView::Construct(&m_hotlist_view, WIDGET_TYPE_CONTACTS_VIEW, PrefsCollectionUI::HotlistContactsSplitter, PrefsCollectionUI::HotlistContactsStyle, PrefsCollectionUI::HotlistContactsManagerSplitter, PrefsCollectionUI::HotlistContactsManagerStyle));
	AddChild(m_hotlist_view, TRUE);
	SetToolbarName("Contacts Panel Toolbar", "Contacts Full Toolbar");
	SetName("Contacts");

	return OpStatus::OK;
}

ContactsPanel::~ContactsPanel()
{
}

void ContactsPanel::OnAdded()
{
}

void ContactsPanel::OnRemoving()
{
}

/***********************************************************************************
**
**	GetPanelText
**
***********************************************************************************/

void ContactsPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_CONTACTS, text);
}

/***********************************************************************************
**
**	OnFullModeChanged
**
***********************************************************************************/

void ContactsPanel::OnFullModeChanged(BOOL full_mode)
{
	m_hotlist_view->SetDetailed(full_mode);
}

void ContactsPanel::OnShow(BOOL show)
{
	if(show && !IsFullMode())
	{
	}
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void ContactsPanel::OnLayoutPanel(OpRect& rect)
{
	m_hotlist_view->SetRect(rect);
}

void ContactsPanel::OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect)
{
	rect = toolbar->LayoutToAvailableRect(rect);
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void ContactsPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_hotlist_view->SetFocus(reason);
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL ContactsPanel::OnInputAction(OpInputAction* action)
{

	HotlistModelItem* item;
	item = (HotlistModelItem*) m_hotlist_view->GetSelectedItem();

	if (item && item->IsContact())
	{
		ContactModelItem* contact = static_cast<ContactModelItem*>(item);
		Index * index = (contact->GetM2IndexId() > 0) ? g_m2_engine->GetIndexById(contact->GetM2IndexId()) : NULL;

		switch (action->GetAction())
		{
			case OpInputAction::ACTION_GET_ACTION_STATE:
			{
				OpInputAction* child_action = action->GetChildAction();

				switch (child_action->GetAction())
				{
					case OpInputAction::ACTION_WATCH_CONTACT:
					{
						if (!index)
							child_action->SetEnabled(TRUE);
						else
						{
							child_action->SetEnabled(!index->IsWatched() && !index->IsIgnored());
							child_action->SetSelected(FALSE);
						}
						return TRUE;
					}

					case OpInputAction::ACTION_STOP_WATCH_CONTACT:
					{
						if (!index)
							child_action->SetEnabled(FALSE);
						else
						{
							child_action->SetEnabled(index->IsWatched() && !index->IsIgnored());
							child_action->SetSelected(index->IsWatched());
						}
						return TRUE;
					}

					case OpInputAction::ACTION_IGNORE_CONTACT:
					{
						if (!index)
							child_action->SetEnabled(TRUE);
						else
						{
							child_action->SetEnabled(!index->IsIgnored() && !index->IsWatched());
							child_action->SetSelected(FALSE);
						}
						return TRUE;
					}

					case OpInputAction::ACTION_STOP_IGNORE_CONTACT:
					{
						if (!index)
							child_action->SetEnabled(FALSE);
						else
						{
							child_action->SetEnabled(index->IsIgnored());
							child_action->SetSelected(index->IsIgnored());
						}
						return TRUE;
					}
					
				}
			break;
			}
		
			case OpInputAction::ACTION_WATCH_CONTACT:
			{
				if (!index)
					index = g_m2_engine->GetIndexById(contact->GetM2IndexId(TRUE));
				if (index)
				{
					index->ToggleWatched(TRUE);
					contact->ChangeImageIfNeeded();
				}
				
				return TRUE;
			}

			case OpInputAction::ACTION_STOP_WATCH_CONTACT:
			{
				if (index)
				{
					index->ToggleWatched(FALSE);
					contact->ChangeImageIfNeeded();
				}
				return TRUE;
			}

			case OpInputAction::ACTION_IGNORE_CONTACT:
			{
				if (!index)
					index = g_m2_engine->GetIndexById(contact->GetM2IndexId(TRUE));
				if (index)
				{
					index->ToggleIgnored(TRUE);
					g_m2_engine->IndexRead(index->GetId());
					contact->ChangeImageIfNeeded();
				}
				return TRUE;
			}

			case OpInputAction::ACTION_STOP_IGNORE_CONTACT:
			{
				if (index)
				{
					index->ToggleIgnored(FALSE);
					contact->ChangeImageIfNeeded();
				}
				return TRUE;
			}

		}
	}
	return m_hotlist_view->OnInputAction(action);
}

INT32 ContactsPanel::GetSelectedFolderID()
{

	return m_hotlist_view->GetSelectedFolderID();
}
