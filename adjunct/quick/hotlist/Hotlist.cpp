/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#include "core/pch.h"
#include "Hotlist.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/panels/BookmarksPanel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/panels/ContactsPanel.h"
#include "adjunct/quick/panels/ExtensionsPanel.h"
#include "adjunct/quick/panels/HistoryPanel.h"
#include "adjunct/quick/panels/WindowsPanel.h"
#include "adjunct/quick/panels/WebPanel.h"
#include "adjunct/quick/panels/TransfersPanel.h"
#include "adjunct/quick/panels/LinksPanel.h"
#include "adjunct/m2_ui/panels/AccordionMailPanel.h"
#include "adjunct/m2_ui/panels/ChatPanel.h"
#include "adjunct/quick/panels/InfoPanel.h"
#include "adjunct/quick/panels/GadgetsPanel.h"
#include "adjunct/quick/panels/UniteServicesPanel.h"
#include "adjunct/quick/panels/NotesPanel.h"
#include "adjunct/quick/panels/StartPanel.h"
#include "adjunct/quick/panels/SearchPanel.h"
#include "adjunct/quick/controller/AddWebPanelController.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/OpLineParser.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "adjunct/desktop_util/sessions/opsession.h"

#include "adjunct/quick/panels/HotlistPanel.h"

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

// order of these strings MUST stay in sync with order of types
// in modules/util/OpTypedObject.h : OpTypedObject::Type
// add new panel types to quick/panel-types.inc
const char* s_panel_types[] =
{
	"Bookmarks",
	"Mail",
	"Contacts",
	"History",
	"Transfers",
	"Links",
	"Windows",
	"Chat",
	"Info",
	"Widgets",
	"Notes",
	"Music",
	"Start",
	"Search",
#ifdef WEBSERVER_SUPPORT
	"Unite",
#endif // WEBSERVER_SUPPORT
	"Extensions",
	NULL
};

///////////////////// HOTLIST  //////////////////////////////////

/***********************************************************************************
 **
 **	Hotlist
 **
 ***********************************************************************************/

Hotlist::Hotlist(PrefsCollectionUI::integerpref prefs_setting) : OpBar(prefs_setting),
	m_selector(NULL)
{
	// needs to happen in constructor, otherwise we won't be able to detect if
	// hotlist is hidden or not
	SetName("Hotlist");
}

void Hotlist::OnDeleted()
{
	g_desktop_bookmark_manager->RemoveBookmarkListener(this);

	if (g_hotlist_manager->GetBookmarksModel())
		g_hotlist_manager->GetBookmarksModel()->RemoveListener(this);
}

OP_STATUS Hotlist::Init()
{
	init_status = OpStatus::ERR;
	GetBorderSkin()->SetImage("Hotlist Skin");
	SetSkinIsBackground(TRUE);

	if (!(m_selector = OP_NEW(HotlistSelector, (this))))
		return OpStatus::ERR_NO_MEMORY;

	m_selector->SetListener(this);
	SetListener(this);
	AddChild(m_selector);

	m_selector->SetStandardToolbar(FALSE);
	m_selector->GetBorderSkin()->SetImage("Hotlist Selector Skin");
	m_selector->SetTabStop(TRUE);
	m_selector->SetName("Hotlist Panel Selector");

	RETURN_IF_ERROR(OpToolbar::Construct(&m_header_toolbar));
	m_header_toolbar->GetBorderSkin()->SetImage("Hotlist Panel Header Skin", "Hotlist Empty Section");
	m_header_toolbar->SetName("Hotlist Panel Header");
	m_header_toolbar->SetWrapping(WRAPPING_OFF);
	m_header_toolbar->SetShrinkToFit(TRUE);

	AddChild(m_header_toolbar);

	if (g_hotlist_manager->GetBookmarksModel()->Loaded())
	{
		OnTreeChanged(g_hotlist_manager->GetBookmarksModel());
		g_hotlist_manager->GetBookmarksModel()->AddListener(this);
	}
	else
	{
		g_desktop_bookmark_manager->AddBookmarkListener(this);
	}

	init_status = OpStatus::OK;
	return init_status;
}

BOOL Hotlist::IsCollapsableToSmall()
{
	return m_selector->IsVertical() && GetResultingAlignment() != ALIGNMENT_FLOATING;
}

OpBar::Collapse Hotlist::TranslateCollapse(Collapse collapse)
{
	if (collapse == COLLAPSE_SMALL && (GetResultingAlignment() == ALIGNMENT_FLOATING || (m_selector && !m_selector->IsVertical())))
		return COLLAPSE_NORMAL;

	return collapse;
}

void Hotlist::OnRelayout()
{
	if (!m_selector)
		return;

	m_selector->SetDeselectable(IsCollapsableToSmall());

	SetCollapse(GetCollapse(), TRUE);

	if (GetCollapse() == COLLAPSE_SMALL)
	{
		m_selector->SetSelected(-1, TRUE);
	}
	else
	{
		if (m_selector->GetSelected() == -1)
		{
			m_selector->SetSelected(ReadActiveTab(), TRUE, TRUE);
		}
	}

	OpBar::OnRelayout();
}

/***********************************************************************************
 **
 **	GetLayout
 **
 ***********************************************************************************/

BOOL Hotlist::GetLayout(OpWidgetLayout& layout)
{
	switch (GetCollapse())
	{
		case COLLAPSE_SMALL:
		{
			layout.SetFixedWidth(m_selector->GetWidthFromHeight(layout.GetAvailableHeight()));
			break;
		}
		case COLLAPSE_NORMAL:
		{
			break;
		}
	}

	return TRUE;
}

/***********************************************************************************
 **
 **	AddPanel
 **
 ***********************************************************************************/

void Hotlist::AddPanel(HotlistPanel* panel, INT32 pos)
{
	panel->SetVisibility(FALSE);
	panel->SetListener(this);

	AddChild(panel);

	if (OpStatus::IsError(panel->Init()))
	{
		panel->Delete();
		return;
	}

	panel->SetFullMode(FALSE, TRUE);
	panel->GetBorderSkin()->SetImage("Hotlist Panel Skin", "Hotlist Empty Section");
	panel->SetSkinIsBackground(TRUE);

	OpString text;
	panel->GetPanelText(text, PANEL_TEXT_LABEL);

	OpButton* button = m_selector->AddButton(text.CStr(), panel->GetPanelImage(), NULL, panel, pos);

	if(button)
	{
		button->SetAttention(panel->GetPanelAttention());

		OpString8 str;
		str.Set("panel_");
		str.Append(panel->GetName());
		button->SetName(str);
		Image img = panel->GetPanelIcon();
		button->GetForegroundSkin()->SetBitmapImage( img, FALSE );
		// Web panels use favicons which can have weird sizes
		button->GetForegroundSkin()->SetRestrictImageSize( panel->GetType() == PANEL_TYPE_WEB );

		panel->SetSelectorButton( button );
	}
	Relayout();
}

/***********************************************************************************
 **
 **	RemovePanel
 **
 ***********************************************************************************/

void Hotlist::RemovePanel(INT32 index)
{
	if (m_selector->GetSelected() == index)
		m_selector->SetSelected(-1, TRUE);

	GetPanel(index)->Delete();
	m_selector->RemoveWidget(index);
}



/***********************************************************************************
 **
 **	MovePanel
 **
 ***********************************************************************************/

void Hotlist::MovePanel(INT32 src, INT32 dst)
{
	m_selector->MoveWidget(src, dst);
}


/***********************************************************************************
 **
 **	GetPanelCount
 **
 ***********************************************************************************/

INT32 Hotlist::GetPanelCount()
{
	return m_selector->GetItemCount();
}



/***********************************************************************************
 **
 **	GetPanel
 **
 ***********************************************************************************/

HotlistPanel* Hotlist::GetPanel(INT32 index)
{

	return (HotlistPanel*) m_selector->GetUserData(index);
}


/***********************************************************************************
 **
 **	GetPanelText
 **
 ***********************************************************************************/

void Hotlist::GetPanelText(INT32 index, OpString& text, PanelTextType text_type)
{
	HotlistPanel* panel = GetPanel(index);

	if (!panel)
		return;

	panel->GetPanelText(text, text_type);
}


/***********************************************************************************
 **
 **	GetPanelImage
 **
 ***********************************************************************************/

OpWidgetImage* Hotlist::GetPanelImage(INT32 index)
{
	OpWidget* widget = m_selector->GetWidget(index);

	if (!widget)
		return NULL;

	return widget->GetForegroundSkin();
}



/***********************************************************************************
 **
 **	GetSelectedPanel
 **
 ***********************************************************************************/

INT32 Hotlist::GetSelectedPanel()
{
	return m_selector->GetSelected();
}



/***********************************************************************************
 **
 **	SetSelectedPanel
 **
 ***********************************************************************************/

void Hotlist::SetSelectedPanel(INT32 index)
{
	m_selector->SetSelected(index, TRUE);
}



/***********************************************************************************
 **
 **	GetPanelByType
 **
 ***********************************************************************************/

HotlistPanel* Hotlist::GetPanelByType(OpTreeModelItem::Type type, INT32* index)
{
	if (!m_selector)
		return NULL;

	for (INT32 i = 0; i < GetPanelCount(); i++)
	{
		if (GetPanel(i)->GetType() == type)
		{
			if (index)
			{
				*index = i;
			}
			return GetPanel(i);
		}
	}

	if (index)
	{
		*index = -1;
	}
	return NULL;
}


/***********************************************************************************
 **
 **	GetPanelNameByType
 **
 ***********************************************************************************/

const char* Hotlist::GetPanelNameByType(Type type)
{
	if (type >= PANEL_TYPE_FIRST && type < PANEL_TYPE_LAST)
	{
		return s_panel_types[type - PANEL_TYPE_FIRST];
	}

	return NULL;
}


/***********************************************************************************
 **
 **	GetPanelTypeByName
 **
 ***********************************************************************************/

OpTypedObject::Type Hotlist::GetPanelTypeByName(const uni_char* name)
{
	INT32 i;
	OpStringC16 name_str(name);

	for (i = 0; name && s_panel_types[i]; i++)
	{
		if (name_str.CompareI(s_panel_types[i]) == 0)
		{
			return (Type) (PANEL_TYPE_FIRST + i);
		}
	}

	return UNKNOWN_TYPE;
}



/***********************************************************************************
 **
 **	ShowPanelByType
 **
 ***********************************************************************************/

BOOL Hotlist::ShowPanelByType(Type type, BOOL show, BOOL focus)
{
	INT32 index = 0;

	BOOL shown = GetPanelByType(type, &index) != NULL;

	if (shown == show)
		return FALSE;

	if (show)
	{
		if (!shown)
		{
			AddPanelByType(type, type - PANEL_TYPE_FIRST);
			m_selector->WriteContent();
		}

		if (focus)
		{
			if (GetPanelByType(type, &index) != NULL)
			{
				m_selector->SetSelected(index, TRUE);
			}
		}

		return !shown;
	}
	else if (shown)
	{
		RemovePanel(index);
		m_selector->WriteContent();
		return TRUE;
	}

	return FALSE;
}


/***********************************************************************************
 **
 **	AddPanelByType
 **
 ***********************************************************************************/

void Hotlist::AddPanelByType(Type type, INT32 pos)
{
	HotlistPanel* panel = CreatePanelByType(type);

	if (!panel)
	{
		return;
	}

	AddPanel(panel, pos);
}



/***********************************************************************************
 **
 **	CreatePanelByType
 **
 ***********************************************************************************/

HotlistPanel* Hotlist::CreatePanelByType(Type type)
{
	switch (type)
	{
		case PANEL_TYPE_BOOKMARKS:
		{
			return OP_NEW(BookmarksPanel, ());
		}
		case PANEL_TYPE_MAIL:
		{
#ifdef M2_SUPPORT
			if (g_application->HasMail() || g_application->HasFeeds())
			{
				return OP_NEW(MailPanel, ());
			}
#endif
			break;
		}
		case PANEL_TYPE_CONTACTS:
		{
#ifdef M2_SUPPORT
			if (g_application->HasMail() || g_application->HasChat())
			{
				return OP_NEW(ContactsPanel, ());
			}
#endif
			break;
		}
		case PANEL_TYPE_HISTORY:
		{
			return OP_NEW(HistoryPanel, ());
		}
		case PANEL_TYPE_TRANSFERS:
		{
			return OP_NEW(TransfersPanel, ());
		}
		case PANEL_TYPE_LINKS:
		{
			return OP_NEW(LinksPanel, ());
		}
		case PANEL_TYPE_WINDOWS:
		{
			return OP_NEW(WindowsPanel, ());
		}
		case PANEL_TYPE_CHAT:
		{
#if defined(M2_SUPPORT) && defined(IRC_SUPPORT)
			if (g_application->HasChat())
			{
				return OP_NEW(ChatPanel, ());
			}
#endif // M2_SUPPORT && IRC_SUPPORT
			break;
		}
		case PANEL_TYPE_INFO:
		{
			return OP_NEW(InfoPanel, ());
		}
		case PANEL_TYPE_NOTES:
		{
			return OP_NEW(NotesPanel, ());
		}
#ifdef WIDGET_RUNTIME_SUPPORT
		case PANEL_TYPE_GADGETS:
		{
			if (!g_pcui->GetIntegerPref(PrefsCollectionUI::DisableWidgetRuntime))
			{
				return OP_NEW(GadgetsPanel, ());
			}
			break;
		}
#endif  // WIDGET_RUNTIME_SUPPORT
#ifdef WEBSERVER_SUPPORT
		case PANEL_TYPE_UNITE_SERVICES:
		{
			if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUnite))
			{
				return OP_NEW(UniteServicesPanel, ());
			}
			break;
		}
#endif // WEBSERVER_SUPPORT
/*		case PANEL_TYPE_START:
		{
			return new StartPanel();
		}*/
		case PANEL_TYPE_SEARCH:
		{
			return OP_NEW(SearchPanel, ());
		}
		case PANEL_TYPE_EXTENSIONS:
		{
			return OP_NEW(ExtensionsPanel, ());
		}
	}
	return NULL;
}


/***********************************************************************************
 **
 **	OnLayout
 **
 ***********************************************************************************/

void Hotlist::OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height)
{
	if (!m_selector)
		return;

	used_width = available_width;
	used_height = available_height;

	if (GetCollapse() == COLLAPSE_SMALL)
	{
		used_width = m_selector->GetWidthFromHeight(available_height);
	}

	if (compute_size_only)
		return;

	OpRect rect(0, 0, available_width, available_height);

	rect = m_selector->LayoutToAvailableRect(rect);
	rect = m_header_toolbar->LayoutToAvailableRect(rect);

	for (INT32 i = 0; i < GetPanelCount(); i++)
	{
		GetPanel(i)->SetRect(rect);
	}
}


/***********************************************************************************
 **
 **	OnChange
 **
 ***********************************************************************************/

void Hotlist::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_selector)
	{
		INT32 selected = m_selector->GetSelected();
		if (selected < GetPanelCount())
		{
			for (INT32 i = 0; i < GetPanelCount(); i++)
			{
				HotlistPanel *panel = GetPanel(i);

				panel->SetVisibility(i == selected);

				// if selector does not have focus, restore focus to previous focuses widget in this panel
				// this is what the user would desire, right?

				if (i == selected && (changed_by_mouse || !m_selector->IsFocused()))
				{
					panel->RestoreFocus(FOCUS_REASON_OTHER);
				}
			}

			if (selected == -1)
			{
				if (IsCollapsableToSmall())
				{
					SetCollapse(COLLAPSE_SMALL, TRUE);

					if (GetWorkspace() && GetWorkspace()->GetActiveDesktopWindow())
					{
						GetWorkspace()->GetActiveDesktopWindow()->RestoreFocus(FOCUS_REASON_OTHER);
					}
				}
				else
				{
					m_selector->SetSelected(ReadActiveTab(), TRUE, TRUE);
				}
			}
			else
			{
				SetCollapse(GetCollapse() == COLLAPSE_SMALL ? COLLAPSE_NORMAL : GetCollapse(), TRUE);
				SaveActiveTab(m_selector->GetSelected());
			}

			g_input_manager->UpdateAllInputStates();

			if (GetParentDesktopWindow())
				GetParentDesktopWindow()->OnChange(this, changed_by_mouse);
		}
	}
	else
	{
		INT32 count = GetPanelCount();

		for (INT32 i = 0; i < count; i++)
		{
			HotlistPanel* panel = GetPanel(i);

			if (widget == panel)
			{
				OpString text;
				panel->GetPanelText(text, PANEL_TEXT_LABEL);

				OpButton* button = (OpButton*) m_selector->GetWidget(i);

				button->SetText(text.CStr());
				button->GetForegroundSkin()->SetImage(panel->GetPanelImage());
				button->SetAttention(panel->GetPanelAttention());

				if (GetParentDesktopWindow() && i == m_selector->GetSelected())
					GetParentDesktopWindow()->OnChange(this, changed_by_mouse);

				return;
			}
		}
	}
}


BOOL Hotlist::ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context)
{
	const char* menu_name = m_selector->GetFocused() != -1 ? "Hotlist Item Popup Menu" : "Hotlist Popup Menu";
	const OpPoint p = point + GetScreenRect().TopLeft();
	g_application->GetMenuHandler()->ShowPopupMenu(menu_name, PopupPlacement::AnchorAt(p, center), 0, use_keyboard_context);
	return TRUE;
}



/***********************************************************************************
 **
 **	AddItem()
 **
 ** @param model_pos - index of item in model
 **
 ** If item at index model_pos in model has set InPanel,
 **   creates a new WebPanel for it, and adds it at position
 **   given in model, or if no such position set, as the last panel
 ** Doesn't check if panel is already in panels
 **
 ** @return TRUE if panel was added, else false.
 **
 ***********************************************************************************/

BOOL Hotlist::AddItem(OpTreeModel* model, INT32 model_pos)
{
	OpTreeModelItem* model_item = model->GetItemByPosition(model_pos);

	HotlistModelItem* hmi = HotlistModel::GetItemByType(model_item);

	if( !hmi->GetInPanel() || hmi->GetIsInsideTrashFolder() )
	{
		return FALSE;
	}

	INT32 panel_position = hmi->GetPanelPos();
	if( panel_position == -1 )
	{
		// This will keep the two lists in sync. I have observed focus problems
		// when this sync'ing was not done [espen 2003-03-29]
		panel_position = GetPanelCount();
		g_hotlist_manager->SetPanelPosition( model_item->GetID(), panel_position, FALSE );
	}

	// Create a WebPanel with ID equal to the model item ID
	WebPanel *new_panel = OP_NEW(WebPanel, ( hmi->GetUrl().CStr(), model_item->GetID(), hmi->GetUniqueGUID().CStr()));
	if (new_panel)
		AddPanel(new_panel, panel_position);

	// As bookmarks loads asynchronously we need to check if the bookmark 
	// just loaded is the ActiveTab
	if (m_selector->GetSelected() == -1 && GetCollapse() != COLLAPSE_SMALL)
	{
		m_selector->SetSelected(ReadActiveTab(), TRUE, TRUE);
	}

	return TRUE;
}

/***********************************************************************************
 **
 **	RemoveItem()
 **
 ** @model_pos - index of item in treemodel
 **
 ** Checks if the item is in panels, and if so, removes it
 **
 ** @return TRUE if panel was (found and) removed, else FALSE
 **
 ***********************************************************************************/

BOOL Hotlist::RemoveItem(OpTreeModel* model, INT32 model_pos)
{
	OpTreeModelItem* model_item = model->GetItemByPosition(model_pos);

	INT32 pos = FindItem(model_item->GetID());

	if (pos != -1)
	{
		RemovePanel(pos);

		return TRUE;
	}

	return FALSE;
}



/***********************************************************************************
 **
 **	ChangeItem()
 **
 ** @param model
 ** @param model_pos - index of item in the model
 **
 **
 ***********************************************************************************/

BOOL Hotlist::ChangeItem(OpTreeModel* model, INT32 model_pos)
{
	if( !model )
	{
		return FALSE;
	}

	OpTreeModelItem* model_item = model->GetItemByPosition(model_pos);

	if( !model_item )
	{
		return FALSE;
	}

	INT32 pos = FindItem(model_item->GetID());

	if (pos == -1)
	{
		return AddItem(model, model_pos);
	}

	// if pos is not -1, that is, if the item is in panels, remoe if neccessary
	HotlistManager::ItemData item_data;
	g_hotlist_manager->GetItemValue( model_item, item_data );
	if( item_data.panel_position == -1 )
	{
		return RemoveItem(model, model_pos);
	}

	// This Panel is already in panels (m_widgets) and should be there

	m_selector->GetWidget(pos)->SetText(item_data.name.CStr());

	// Move the panel to the position given in bookmarks model
	m_selector->MoveWidget(pos, item_data.panel_position);


	for (INT32 i = 0; i < GetPanelCount(); i++)
	{
		HotlistPanel* panel = GetPanel(i);
		if (panel->GetType() == PANEL_TYPE_WEB
			&& model_item->GetID() == panel->GetID())
		{
			panel->UpdateSettings();
			return TRUE;
		}
	}

	return TRUE;
}



/***********************************************************************************
 **
 **	FindItem()
 **
 ** @param id - ID of item in the hotlist model
 **
 ** Walks through all panels and for each of the panels that are web panels,
 ** checks if the panel id is same as param id.
 **
 ** @return index in m_widgets (WidgetVector of Selector (OpToolbar))
 **         -1 if item not found
 **
 ***********************************************************************************/

INT32 Hotlist::FindItem(INT32 id)
{
	for (INT32 i = 0; i < GetPanelCount(); i++)
	{
		HotlistPanel* panel = GetPanel(i);

		if (panel->GetType() == PANEL_TYPE_WEB && id == panel->GetID())
		{
			return i;
		}
	}

	return -1;
}

INT32 Hotlist::FindItem(OpString guid)
{
	for (INT32 i = 0; i < GetPanelCount(); i++)
	{
		HotlistPanel* panel = GetPanel(i);

		if (panel->GetType() == PANEL_TYPE_WEB && guid.CompareI(((WebPanel*)panel)->GetGuid()) == 0)
		{
			return i;
		}
	}

	return -1;

}

/***********************************************************************************
 **
 **	OnItemAdded
 **
 ***********************************************************************************/

void Hotlist::OnItemAdded(OpTreeModel* tree_model, INT32 pos)
{
	if (AddItem(tree_model, pos))
	{
		Relayout();
		//save changements in panel order
		m_selector->WriteContent();
	}
}



/***********************************************************************************
 **
 **	OnItemRemoving
 **
 ***********************************************************************************/

void Hotlist::OnItemRemoving(OpTreeModel* tree_model, INT32 pos)
{
	if (RemoveItem(tree_model, pos))
	{
		Relayout();
		//save changements in panel order
		m_selector->WriteContent();
	}
}

/***********************************************************************************
 **
 **	OnItemChanged
 **
 ***********************************************************************************/

void Hotlist::OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort)
{
	if (ChangeItem(tree_model, pos))
	{
		Relayout();
		//save changements in panel order
		m_selector->WriteContent();
	}
}

/***********************************************************************************
 **
 **	OnTreeChanged
 **
 ***********************************************************************************/

void Hotlist::OnTreeChanged(OpTreeModel* tree_model)
{
	OP_ASSERT(tree_model == g_hotlist_manager->GetBookmarksModel());
	if (!tree_model) return;

	// we should all go away when we get proper bookmark ids... I don't like this mess.
	// Vector items hold indexes in the bookmarks model
	INT32 i;
	OpINT32Vector items;
	BOOL found = FALSE;

	for (i = 0; i < tree_model->GetItemCount(); i++)
	{
		HotlistModelItem* item = g_hotlist_manager->GetBookmarksModel()->GetItemByIndex(i);
		if (!item->GetInPanel())
			continue;

		// Set found to true if a bookmark is in a panel
		found = TRUE;

		if (item->GetPanelPos() == -1)
		{
			//No panel position set, put as last
			items.Add(i);
		}
		else
		{
			// if the item already is in the panel list, move to correct position
			if (FindItem(item->GetID()) >= 0)
			{
				ChangeItem(tree_model, item->GetIndex());
				continue;
			}

			// position is not -1 and item is not already in panel list
			// Add the bookmark to vector before any item with a higher panel position
			INT32 j;
			for (j = 0; j < (INT32) items.GetCount(); j++)
			{
				HotlistModelItem* other_item = g_hotlist_manager->GetBookmarksModel()->GetItemByIndex(items.Get(j));

				if (other_item->GetPanelPos() > item->GetPanelPos())
					break;
			}

			items.Insert(j, i);
		}
	}

	//add the bookmarks (based on their id's) as webpanels
	for (i = 0; i < (INT32) items.GetCount(); i++)
	{
		AddItem(tree_model, items.Get(i)); // items.Get(i) is index i bookmarks model of item
	}

	//save changements in panel order
	if (found)
		m_selector->WriteContent();
}

/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL Hotlist::OnInputAction(OpInputAction* action)
{
	if (!m_selector)
		return FALSE;

	if (OpBar::OnInputAction(action))
		return TRUE;

	HotlistPanel* panel = m_selector->GetFocused() != -1 ? GetPanel(m_selector->GetFocused()) : NULL;
	WebPanel* web_panel = panel && panel->GetType() == PANEL_TYPE_WEB ? (WebPanel*) panel : NULL;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_FOCUS_PANEL:
				{
					child_action->SetEnabled(TRUE);
					child_action->SetSelected(!child_action->HasActionDataString() && m_selector->GetSelected() == child_action->GetActionData());
					return TRUE;
				}

				case OpInputAction::ACTION_EDIT_PANEL:
				{
					child_action->SetEnabled(web_panel != NULL);
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_POPUP_MENU:
				{
					if (child_action->HasActionDataString() && uni_stricmp(child_action->GetActionDataString(), UNI_L("Internal Panels")) == 0)
					{
						OpString text;
						GetPanelText(m_selector->GetSelected(), text, PANEL_TEXT_NAME);

						child_action->SetActionText(text.CStr());
						return TRUE;
					}
					return FALSE;
				}
				case OpInputAction::ACTION_ADD_WEB_PANEL:
				{
					// DSK-351304: adding a web panel creates new bookmark - disable until bookmarks are loaded
					child_action->SetEnabled(g_desktop_bookmark_manager->GetBookmarkModel()->Loaded());
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			if (action->GetActionData() == WIDGET_TYPE_HOTLIST)
			{
				action->SetActionObject(this);
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_HIDE_PANEL:
		{
			if (action->GetActionData() == -1 && IsCollapsableToSmall())
			{
				return m_selector->SetSelected(-1, TRUE);
			}

			BOOL has_focus = IsFocused(TRUE);
			BOOL result = ShowPanelByName(action->GetActionDataString(), FALSE);

			if( has_focus )
			{
				DesktopWindow *window = g_application->GetActiveDesktopWindow();
				if( window )
				{
					//Does not work. Would like to use it but does not work yet [espen 2004-04-01]
					//window->RestoreFocus(FOCUS_REASON_OTHER);
					g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
				}
			}

			return result;
		}

		case OpInputAction::ACTION_SHOW_PANEL:
		case OpInputAction::ACTION_FOCUS_PANEL:
		{
			INT32 index = action->GetActionData();

			HotlistPanel* panel = NULL;
			BOOL handled = FALSE;

			if (action->HasActionDataString()) // show a specific panel (another panel might already be open)
			{
				handled = ShowPanelByName(action->GetActionDataString(), TRUE);

				if (!handled && action->GetAction() == OpInputAction::ACTION_SHOW_PANEL)
					return FALSE;

				panel = GetPanelByType(GetPanelTypeByName(action->GetActionDataString()), &index);
			}
			else if (action->GetAction() == OpInputAction::ACTION_FOCUS_PANEL ||	// focus a specific panel
					 GetAlignment() == ALIGNMENT_OFF || GetSelectedPanel() == -1)	// show the currently active panel (no panel is currently open)
			{
				if (index == -1)
					index = ReadActiveTab();

				panel = GetPanel(index);
			}
			else // don't show anything (advance to next input action, which is normally: hide panel)
			{
				action->SetWasReallyHandled(FALSE);
				return TRUE;
			}

			if (!panel)
				return FALSE;

			// make sure we are visible

			if (SetAlignment(OpBar::ALIGNMENT_OLD_VISIBLE))
				handled = TRUE;

			SyncLayout();

			if (m_selector->SetSelected(index, TRUE, TRUE))
				handled = TRUE;

			if (GetParentDesktopWindow()->Activate())
				handled = TRUE;

			panel->RestoreFocus(FOCUS_REASON_ACTION);

			return handled;
		}

		case OpInputAction::ACTION_REMOVE_PANEL:
		{
			if (web_panel)
			{
				g_hotlist_manager->ShowInPanel( web_panel->GetID(), FALSE );
			}
			else if (panel)
			{
				RemovePanel(m_selector->GetFocused());
				m_selector->WriteContent();
			}
			return TRUE;
		}

		case OpInputAction::ACTION_EDIT_PANEL:
		{
			if (web_panel)
			{
				g_hotlist_manager->EditItem( web_panel->GetID(), GetParentDesktopWindow() );
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
		{
			ShowContextMenu(m_selector->GetBounds().Center(),TRUE,TRUE);
			return TRUE;
		}

		case OpInputAction::ACTION_ADD_WEB_PANEL:
		{
			AddWebPanelController* controller = OP_NEW(AddWebPanelController, ());
			OpStatus::Ignore(ShowDialog(controller, g_global_ui_context, g_application->GetActiveDocumentDesktopWindow()));
			return TRUE;
		}

		case OpInputAction::ACTION_MANAGE:
		{
			if (action->HasActionDataString() || !panel)
				return FALSE;

			if (web_panel)
			{
				OpenURLSetting settings;
				web_panel->GetURL(settings.m_address);
				settings.m_ignore_modifier_keys = (action->IsKeyboardInvoked());

				g_application->OpenURL(settings);
			}
			else
			{
				g_application->GetPanelDesktopWindow(panel->GetType() , TRUE);
			}
			return TRUE;
		}
	}

	return FALSE;
}


/***********************************************************************************
 **
 **	OnReadPanels
 **
 ***********************************************************************************/

void Hotlist::OnReadPanels(PrefsSection *section)
{
	OP_PROFILE_METHOD("Created panels");

	//remember which panels exist right now, so we know what to remove
	OpVector<HotlistPanel> removed_panels;

	for (INT32 i=0; i < GetPanelCount(); ++i)
	{
		HotlistPanel* old_panel = GetPanel(i);
		if (old_panel->GetType() != PANEL_TYPE_WEB)	//web panels are maintained by the bookmarks events
			removed_panels.Add(old_panel);
	}

	//read panel setup and apply to current setup
	if (section)
	{
		for (const PrefsEntry *entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
		{
			OpLineParser line(entry->Key());

			OpString item_type;
			INT32 pos = -1;

			if (OpStatus::IsError(line.GetNextToken(item_type)))
				continue;

			line.GetNextValue(pos);

			//get the new panel type;
			Type new_panel_type = GetPanelTypeByName(item_type.CStr());

			//move or create it if needed
			HotlistPanel* positioned_panel = PositionPanel(new_panel_type, pos);

			//this panel (if already existed) should not be removed
			OpStatus::Ignore(removed_panels.RemoveByItem(positioned_panel));
		}
	}

	//all panels in this list have not been reread, so they must have been removed
	while (removed_panels.GetCount() > 0)
	{
		INT32 pos;
		GetPanelByType(removed_panels.Get(0)->GetType(), &pos);
		OP_ASSERT(pos >= 0);	//panels in the removed_panels list should exist
		//delete panel and remove from the removal list
		RemovePanel(pos);
		removed_panels.Remove(0);
	}

	//update panels from bookmarks (webpanels)
	OnTreeChanged(g_hotlist_manager->GetBookmarksModel());

	INT32 active = ReadActiveTab();

	Collapse collapse = GetCollapse();
	if (collapse == COLLAPSE_SMALL)
		active = -1;

	m_selector->SetSelected(active, TRUE, TRUE);
}



/***********************************************************************************
 **
 **	PositionPanel
 **
 ** @param panel_type
 ** @param pos - position to place panel in
 **
 ** If pos is < 0, panel is added last
 ** If this type of item is already there, move it to position pos
 ** If a panel of this type is not already there, add such a panel in position pos
 **
 ***********************************************************************************/

HotlistPanel* Hotlist::PositionPanel(Type panel_type, INT32 pos)
{
	OP_ASSERT(panel_type != PANEL_TYPE_WEB);	//this function current does not support
	                                            // web panels (because they can have multiple instances)

	Type current_panel_type = OpTypedObject::UNKNOWN_TYPE;

	//get current panel on target position
	HotlistPanel* current_panel = GetPanel(pos);

	if (current_panel)
	{
		current_panel_type = current_panel->GetType();
	}

	if (panel_type == current_panel_type)
	{
		//same panel is already there, do nothing
		return current_panel;
	}

	//If the position is not defined, set it on default as last
	if (pos < 0)
	{
		pos = GetPanelCount();
	}

	//Look if this type of panel is already there
	INT32	dub_pos;
	HotlistPanel* same_panel = GetPanelByType(panel_type, &dub_pos);
	if (same_panel)
	{
		//this type of panel is already somewhere else, let's move it to new position
		MovePanel(dub_pos, pos);
		return same_panel;
	}

	//A panel of panel_type currently doesn't exists -> add a new panel to the desired position
	AddPanelByType(panel_type, pos);

	return GetPanel(pos);
}


/***********************************************************************************
 **
 **	OnWritePanels
 **
 ***********************************************************************************/

void Hotlist::OnWritePanels(PrefsFile* prefs_file, const char* name)
{
	for (INT32 i = 0; i < GetPanelCount(); i++)
	{
		HotlistPanel* panel = GetPanel(i);

		if (panel->GetType() == PANEL_TYPE_WEB)
		{
			g_hotlist_manager->SetPanelPosition( panel->GetID(), i, FALSE );
		}
		else
		{
			OpLineString key;
			OpString name_str;

			key.WriteToken8(GetPanelNameByType(panel->GetType()));
			key.WriteValue(i, FALSE);

			name_str.Set(name);
			prefs_file->WriteStringL(name_str.CStr(), key.GetString(), NULL);
		}
	}
}


/***********************************************************************************
 **
 **	OnFocus
 **
 ***********************************************************************************/

void Hotlist::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	if (focus)
		m_selector->SetFocus(reason);
}


/***********************************************************************************
 **
 **	OnDragStart
 **
 ***********************************************************************************/

void Hotlist::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (widget == m_selector && g_application->IsDragCustomizingAllowed())
	{
		HotlistPanel* panel = GetPanel(pos);
		if (!panel)
			return;

		DesktopDragObject* drag_object = m_selector->GetWidget(pos)->GetDragObject(panel->GetType() == PANEL_TYPE_WEB ? DRAG_TYPE_BOOKMARK : panel->GetType(), x, y);

		if (drag_object)
		{
			if (panel->GetType() == PANEL_TYPE_WEB)
			{
				HotlistManager::ItemData item_data;
				HotlistModelItem* item = g_hotlist_manager->GetItemByID(panel->GetID());
				if (item->GetModel() && item->GetModel()->GetModelType() == HotlistModel::BookmarkRoot && item->IsFolder())
					return;

				if( !g_hotlist_manager->GetItemValue(panel->GetID(), item_data ))
				{
					return;
				}
				else
				{
					drag_object->AddID(panel->GetID());
					drag_object->SetURL(item_data.url.CStr());
					drag_object->SetTitle(item_data.name.CStr());
				}
			}
			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
}

/***********************************************************************************
 **
 **	OnDragMove
 **
 ***********************************************************************************/

void Hotlist::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	if (widget == m_selector)
	{
		if (drag_object->GetType() == DRAG_TYPE_BOOKMARK)
		{
			DropType drop_type = DROP_NOT_AVAILABLE;
			HotlistManager::ItemData item_data;
			HotlistModelItem* item = g_hotlist_manager->GetItemByID(drag_object->GetID(0));
			// Folder should not be dropped on panel
			if( g_hotlist_manager->GetItemValue(drag_object->GetID(0), item_data) && !item->IsFolder() && !item->GetIsInsideTrashFolder())
			{
				drop_type = (item_data.panel_position >= 0) ? DROP_MOVE : DROP_COPY;
			}
			drag_object->SetDesktopDropType(drop_type);
		}
		else if (drag_object->GetType() >= PANEL_TYPE_FIRST && drag_object->GetType() < PANEL_TYPE_LAST)
		{
			INT32 index;

			BOOL copy = GetPanelByType(drag_object->GetType(), &index) == NULL;

			drag_object->SetDesktopDropType(copy ? DROP_COPY : DROP_MOVE);
		}
		else if (drag_object->GetType() == DRAG_TYPE_HISTORY)
		{
			DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

			if( drag_object->GetIDCount() > 0 )
			{
				HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(0));

				if (history_item)
				{
					BookmarkItemData item_data;
					item_data.panel_position = pos;
					INT32 target_id = HotlistModel::BookmarkRoot;
					if( g_desktop_bookmark_manager->DropItem( item_data, target_id, DesktopDragObject::INSERT_AFTER, FALSE,drag_object->GetType() ) )
					{
						drag_object->SetDesktopDropType(DROP_COPY);
					}
				}
			}
		}
		else if (drag_object->GetURL())
		{
			if( !*drag_object->GetURL() )
			{
				drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
				return;
			}
			BookmarkItemData item_data;
			item_data.panel_position = pos;
			INT32 target_id = HotlistModel::BookmarkRoot;
			if( g_desktop_bookmark_manager->DropItem( item_data, target_id, DesktopDragObject::INSERT_AFTER, FALSE, drag_object->GetType() ) )
			{
				drag_object->SetDesktopDropType(DROP_COPY);
			}
		}
	}
}

/***********************************************************************************
 **
 **	OnDragDrop
 **
 ***********************************************************************************/

void Hotlist::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (widget == m_selector)
	{
		if (drag_object->GetType() == DRAG_TYPE_BOOKMARK)
		{
			for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
			{
				g_hotlist_manager->SetPanelPosition( drag_object->GetID(i), pos, TRUE );
			}
		}
		else if (drag_object->GetType() >= PANEL_TYPE_FIRST && drag_object->GetType() < PANEL_TYPE_LAST)
		{
			INT32 index;

			BOOL copy = GetPanelByType(drag_object->GetType(), &index) == NULL;

			if (copy)
			{
				AddPanelByType(drag_object->GetType(), pos);
			}
			else
			{
				m_selector->MoveWidget(index, pos);
			}
		}
		else if (drag_object->GetType() == DRAG_TYPE_HISTORY)
		{
			DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

			for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
			{
				HistoryModelItem* item = history_model->FindItem(drag_object->GetID(i));
				HistoryModelPage* history_item = (HistoryModelPage*) item;

				if (history_item)
				{
					BookmarkItemData item_data;

					OpString address;
					OpString title;

					history_item->GetAddress(address);
					history_item->GetTitle(title);

					item_data.name.Set( title.CStr() );
					item_data.url.Set( address.CStr() );
					item_data.panel_position = pos;
					INT32 target_id = HotlistModel::BookmarkRoot;

					g_desktop_bookmark_manager->DropItem( item_data, target_id, DesktopDragObject::INSERT_AFTER, TRUE, drag_object->GetType() );

				}
			}
		}
		else if (drag_object->GetURL())
		{
			BookmarkItemData item_data;
			item_data.name.Set( drag_object->GetTitle() );
			item_data.url.Set( drag_object->GetURL() );
			item_data.panel_position = pos;
			INT32 target_id = HotlistModel::BookmarkRoot;
			g_desktop_bookmark_manager->DropItem( item_data, target_id, DesktopDragObject::INSERT_AFTER, TRUE, drag_object->GetType() );
		}
		m_selector->WriteContent();
		SaveActiveTab(m_selector->GetSelected());
	}
}

void Hotlist::SaveActiveTab(UINT index)
{
	HotlistPanel* panel = GetPanel(index);
	if (panel)
	{
		if (panel->GetType() == PANEL_TYPE_WEB)
		{
			TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::HotlistActiveTab, ((WebPanel*)panel)->GetGuid()));
		}
		else
		{
			OpString panel_name;
			panel_name.Set(GetPanelNameByType(panel->GetType()));
			TRAPD(err, g_pcui->WriteStringL(PrefsCollectionUI::HotlistActiveTab, panel_name));
		}
	}
}

INT32 Hotlist::ReadActiveTab()
{
	INT32 index = -1;
	OpString active;
	active.Set(g_pcui->GetStringPref(PrefsCollectionUI::HotlistActiveTab));
	GetPanelByType(GetPanelTypeByName(active.CStr()), &index);

	if (index != -1)
	{
		return index;
	}
	else
	{
		// This is a bookmark guid
		index = FindItem(active);
		if(index < 0)
		{
			// floating panels must have an active item
			if(GetResultingAlignment() == ALIGNMENT_FLOATING)
			{
				index = 0;
				SaveActiveTab(index);
			}
		}
		return index;
	}
}


void Hotlist::OnBookmarkModelLoaded()
{
	OnTreeChanged(g_hotlist_manager->GetBookmarksModel());
	g_hotlist_manager->GetBookmarksModel()->AddListener(this);
}

///////////////////// HOTLIST SELECTOR  //////////////////////////////////

/***********************************************************************************
 **
 **	HotlistSelector
 **
 ***********************************************************************************/

HotlistSelector::HotlistSelector(Hotlist* hotlist) :
m_hotlist(hotlist)
{
	SetSelector(TRUE);
	SetGrowToFit(TRUE);	// keep them from growing and forever hiding the + button when aligned to the top or bottom
	SetShrinkToFit(TRUE);

	OP_STATUS status = OpToolbar::Construct(&m_floating_bar);
	CHECK_STATUS(status);

	m_floating_bar->GetBorderSkin()->SetImage("Hotlist Floating Skin");
	m_floating_bar->SetName("Hotlist Floating");

	AddChild(m_floating_bar);
}

void HotlistSelector::OnRelayout()
{
	OpToolbar::OnRelayout();

	Alignment alignment = GetResultingAlignment();

	// floating toolbar gets the same alignment as the HotlistSelector in order to define
	// its skinning based on the hotlist selector's position and make sure buttons added to it
	// are aligned the same way as the parent toolbar
	if (alignment != ALIGNMENT_OFF)
	{
		if (m_floating_bar->GetAlignment() != ALIGNMENT_OFF)
			m_floating_bar->SetAlignment(alignment);
	}
}


void HotlistSelector::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	OpToolbar::GetPadding(left, top, right, bottom);
	if (IsHorizontal())
	{
		// if the floating bar is shown and the selector is horizontal, right-align the floating bar and grow-to-fit the rest of the buttons
		if ((m_floating_bar->IsOn() && m_floating_bar->GetWidgetCount() > 0) ||
			g_application->IsCustomizingHiddenToolbars())
		{
			// make space available for the floating toolbar
			OpRect floating_rect;
			m_floating_bar->GetRequiredSize(floating_rect.width, floating_rect.height);

			*right += floating_rect.width;
		}
	}

}

BOOL HotlistSelector::SetAlignment(Alignment alignment, BOOL write_to_prefs)
{
	BOOL rc = OpToolbar::SetAlignment(alignment, write_to_prefs);

	SkinType skin_type = GetBorderSkin()->GetType();
	SetButtonSkinType(skin_type);

	return rc;
}

void HotlistSelector::OnLayout()
{
	OpBar::OnLayout(); // needs to be done before the floating bar is positioned

	// if the floating bar is shown
	if ((m_floating_bar->IsOn() && m_floating_bar->GetWidgetCount() > 0) ||
		g_application->IsCustomizingHiddenToolbars())
	{
		INT32 used_width = 0, used_height = 0;

		OpRect floating_rect;
		INT32 left, top, right, bottom;

		OpToolbar::GetPadding(&left, &top, &right, &bottom);

		// layout the floating toolbar to be within the padding of this toolbar
		OpRect rect = GetBounds();
		OpToolbar::OnLayout(TRUE, rect.width, rect.height, used_width, used_height);

		rect.x += left;
		rect.y += top;
		rect.width -= left + right;
		rect.height -= top + bottom;

		m_floating_bar->GetRequiredSize(floating_rect.width, floating_rect.height);

		INT32 pos = GetWidgetCount();

		OpWidget *last_button = pos ? GetWidget(pos - 1) : NULL;

		if(IsHorizontal())
		{
			INT32 w, row_height;
			GetBorderSkin()->GetSize(&w, &row_height);
			if (row_height > floating_rect.height)
				floating_rect.height = row_height;

			// align after the last button, if any
			if(last_button)
			{
				OpRect b_rect = last_button->GetRect();

				floating_rect.x = rect.width - floating_rect.width; //b_rect.Right() + 1;
				floating_rect.y = b_rect.y + (b_rect.height - floating_rect.height) / 2;
			}
			else
			{
				floating_rect.x = 0;
				floating_rect.y = rect.y + (rect.height - floating_rect.height) / 2;
			}
		}
		else // isVertical
		{
			// right-align the floating bar and grow-to-fit the rest of the buttons (done automatically for vertical toolbars?)
			// TODO: Margins and paddings
			floating_rect.x = rect.x + (rect.width - floating_rect.width) / 2;
			if(last_button)
			{
				OpRect b_rect = last_button->GetRect();

				floating_rect.y = b_rect.Bottom() + 1;
			}
			else
			{
				floating_rect.y = 0;
			}
		}
		m_floating_bar->LayoutToAvailableRect(floating_rect);
	}
}

/***********************************************************************************
 **
 **	MoveWidget
 **
 ***********************************************************************************/

void HotlistSelector::MoveWidget(INT32 src, INT32 dst)
{
	OpToolbar::MoveWidget(src, dst);
}

/**
 * SetSelected
 *
 */
BOOL HotlistSelector::SetSelected(INT32 index, BOOL invoke_listeners, BOOL changed_by_mouse)
{
	return OpToolbar::SetSelected(index, invoke_listeners, changed_by_mouse);
}

/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL HotlistSelector::OnInputAction(OpInputAction* action)
{
	if (OpToolbar::OnInputAction(action))
		return TRUE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			if (action->GetActionData() == WIDGET_TYPE_PANEL_SELECTOR)
			{
				action->SetActionObject(this);
				return TRUE;
			}
			return FALSE;
		}
	}

	return FALSE;
}


/***********************************************************************************
 **
 **	OnClick
 **
 ***********************************************************************************/
void HotlistSelector::OnClick(OpWidget *widget, UINT32 id)
{
	OpToolbar::OnClick(widget, id);
}


/***********************************************************************************
 **
 **	OnContextMenu
 **
 ***********************************************************************************/

BOOL HotlistSelector::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	if (widget == m_floating_bar)
	{
		g_application->GetMenuHandler()->ShowPopupMenu(child_index == -1 ? "Toolbar Popup Menu" : "Toolbar Item Popup Menu",
		                                               PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
		return TRUE;
	}
	else if( widget == this )
	{
		g_application->GetMenuHandler()->ShowPopupMenu(child_index != -1 && child_index < GetItemCount() ? "Hotlist Item Popup Menu" : "Hotlist Popup Menu",
		                                               PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
		return TRUE;
	}
	else
	{
		return OpToolbar::OnContextMenu(widget, child_index, menu_point, avoid_rect, keyboard_invoked);
	}
}

/***********************************************************************************
 **
 **	GetItemCount
 **
 ***********************************************************************************/
INT32 HotlistSelector::GetItemCount()
{
	return GetWidgetCount();
}
