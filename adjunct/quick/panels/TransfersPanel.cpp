/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/panels/TransfersPanel.h"

#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/actions/action_utils.h"

#include "modules/widgets/OpWidgetFactory.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/util/filefun.h"
#include "modules/dragdrop/dragdrop_manager.h"

#ifdef _MACINTOSH_
# include "platforms/mac/util/MacIcons.h"
#endif


/***********************************************************************************
**
**
**
***********************************************************************************/
TransfersPanel::TransfersPanel()
{
	g_desktop_transfer_manager->AddTransferListener(this);
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnDeleted()
{
	g_desktop_transfer_manager->RemoveTransferListener(this);
	m_transfers_view->SetTreeModel(NULL);

	HotlistPanel::OnDeleted();
}


/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS TransfersPanel::Init()
{
	RETURN_IF_ERROR(OpTreeView::Construct(&m_transfers_view));
	m_transfers_view->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	
	AddChild(m_transfers_view, TRUE);

	RETURN_IF_ERROR(OpGroup::Construct(&m_item_details_group));
	AddChild(m_item_details_group, TRUE);
	m_item_details_group->SetSkinned(TRUE);
	m_item_details_group->GetBorderSkin()->SetImage("Transfer Panel Details Skin", "Window Skin");

	RETURN_IF_ERROR(OpLabel::Construct(&m_item_details_from_label));
	m_item_details_from_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_from_label, TRUE);
	RETURN_IF_ERROR(OpLabel::Construct(&m_item_details_to_label));
	m_item_details_to_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_to_label, TRUE);
	RETURN_IF_ERROR(OpLabel::Construct(&m_item_details_size_label));
	m_item_details_size_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_size_label, TRUE);
	RETURN_IF_ERROR(OpLabel::Construct(&m_item_details_transferred_label));
	m_item_details_transferred_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_transferred_label, TRUE);
	RETURN_IF_ERROR(OpLabel::Construct(&m_item_details_connections_label));
	m_item_details_connections_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_connections_label, TRUE);
	RETURN_IF_ERROR(OpLabel::Construct(&m_item_details_activetransfers_label));
	m_item_details_activetransfers_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_activetransfers_label, TRUE);
	RETURN_IF_ERROR(OpEdit::Construct(&m_item_details_from_info_label));
	m_item_details_from_info_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_from_info_label->SetForceTextLTR(TRUE);
	m_item_details_group->AddChild(m_item_details_from_info_label, TRUE);
	m_item_details_from_info_label->SetFlatMode();
	RETURN_IF_ERROR(OpEdit::Construct(&m_item_details_to_info_label));
	m_item_details_to_info_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_to_info_label->SetForceTextLTR(TRUE);
	m_item_details_group->AddChild(m_item_details_to_info_label, TRUE);
	m_item_details_to_info_label->SetFlatMode();
	RETURN_IF_ERROR(OpEdit::Construct(&m_item_details_size_info_label));
	m_item_details_size_info_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_size_info_label, TRUE);
	m_item_details_size_info_label->SetFlatMode();
	RETURN_IF_ERROR(OpEdit::Construct(&m_item_details_transferred_info_label));
	m_item_details_transferred_info_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_transferred_info_label, TRUE);
	m_item_details_transferred_info_label->SetFlatMode();
	RETURN_IF_ERROR(OpEdit::Construct(&m_item_details_connections_info_label));
	m_item_details_connections_info_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_connections_info_label, TRUE);
	m_item_details_connections_info_label->SetFlatMode();
	RETURN_IF_ERROR(OpEdit::Construct(&m_item_details_activetransfers_info_label));
	m_item_details_activetransfers_info_label->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_item_details_group->AddChild(m_item_details_activetransfers_info_label, TRUE);
	m_item_details_activetransfers_info_label->SetFlatMode();

	SetToolbarName("Transfers Panel Toolbar", "Transfers Full Toolbar");
	SetName("Transfers");

	m_showdetails = g_pcui->GetIntegerPref(PrefsCollectionUI::TransWinShowDetails);

	m_show_extra_info = FALSE;

	m_transfers_view->SetListener(this);

	OpString ui_text;

	g_languageManager->GetString(Str::DI_IDL_SOURCE, ui_text);
	m_item_details_from_label->SetLabel(ui_text.CStr());

	g_languageManager->GetString(Str::DI_IDL_DESTINATION, ui_text);
	m_item_details_to_label->SetLabel(ui_text.CStr());

	g_languageManager->GetString(Str::DI_IDL_SIZE, ui_text);
	m_item_details_size_label->SetLabel(ui_text.CStr());

	g_languageManager->GetString(Str::DI_IDL_TRANSFERED, ui_text);
	m_item_details_transferred_label->SetLabel(ui_text.CStr());

	g_languageManager->GetString(Str::S_TRANSFERS_PANEL_CONNECTION_SHORT, ui_text);
	m_item_details_connections_label->SetLabel(ui_text.CStr());

	g_languageManager->GetString(Str::S_TRANSFERS_PANEL_ACTIVE_TRANSFERS, ui_text);
	m_item_details_activetransfers_label->SetLabel(ui_text.CStr());

	m_transfers_view->SetMultiselectable(TRUE);
#ifdef WINDOW_COMMANDER_TRANSFER
	m_transfers_view->SetTreeModel((OpTreeModel *)g_desktop_transfer_manager);
#endif // WINDOW_COMMANDER_TRANSFER

	m_transfers_view->SetColumnWeight(1, 250);
	m_transfers_view->SetColumnFixedWidth(0, 18);
	m_transfers_view->SetVisibility(TRUE);

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_BOOKMARK)
	{
		m_transfers_view->SetDragEnabled(TRUE);
	}

	m_transfers_view->SetSelectedItem(g_pcui->GetIntegerPref(PrefsCollectionUI::TransferItemsAddedOnTop) ? 0 : m_transfers_view->GetItemCount() - 1 );
	m_transfers_view->SetFocus(FOCUS_REASON_OTHER);

	m_item_details_from_info_label->SetListener(&m_widgetlistener);
	m_item_details_to_info_label->SetListener(&m_widgetlistener);
	m_item_details_size_info_label->SetListener(&m_widgetlistener);
	m_item_details_transferred_info_label->SetListener(&m_widgetlistener);
	m_item_details_connections_info_label->SetListener(&m_widgetlistener);
	m_item_details_activetransfers_info_label->SetListener(&m_widgetlistener);

	m_transfers_view->SetThreadColumn(1);

	return OpStatus::OK;
}


/***********************************************************************************
**
**	GetPanelText
**
***********************************************************************************/
void TransfersPanel::GetPanelText(OpString& text,
								  Hotlist::PanelTextType text_type)
{
	OpString remaining;

	g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_TRANSFERS, text);
	GetTimeRemainingText(remaining);

	if (remaining.HasContent())
	{
		switch (text_type)
		{
			case Hotlist::PANEL_TEXT_LABEL:
			{
				text.Set(remaining.CStr());
				break;
			}

			case Hotlist::PANEL_TEXT_TITLE:
			{
				text.Append(" ");
				text.Append(remaining.CStr());
				break;
			}
		}
	}
}


/***********************************************************************************
**
**	GetPanelAttention
**
***********************************************************************************/
BOOL TransfersPanel::GetPanelAttention()
{
	return g_desktop_transfer_manager->IsNewTransfersDone();
}


/***********************************************************************************
**
**	OnFullModeChanged
**
***********************************************************************************/
void TransfersPanel::OnFullModeChanged(BOOL full_mode)
{
	m_transfers_view->SetShowColumnHeaders(full_mode);
	m_transfers_view->SetColumnVisibility(0, full_mode);
	m_transfers_view->SetColumnVisibility(2, full_mode);
	m_transfers_view->SetColumnVisibility(3, full_mode);
	m_transfers_view->SetColumnVisibility(4, full_mode);
	m_transfers_view->SetColumnVisibility(5, full_mode);
	m_transfers_view->SetColumnVisibility(6, !full_mode);
	m_transfers_view->SetName(full_mode ? "Transfers View" : "Transfers View Small");
	// m_transfers_view->SetLinkColumn(IsSingleClick() ? 0 : -1);

	if(full_mode)
	{
		m_transfers_view->GetBorderSkin()->SetImage("Detailed Panel Treeview Skin", "Treeview Skin");
	}
	else
	{
		m_transfers_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");
	}
}


/***********************************************************************************
**
**	OnLayoutPanel
**
***********************************************************************************/
void TransfersPanel::OnLayoutPanel(OpRect& rect)
{
	int top_margin = m_show_extra_info ? 120 : 80;

	m_transfers_view->SetRect(OpRect(rect.x, rect.y, rect.width, rect.height - (m_showdetails ? top_margin : 0)));
	m_item_details_group->SetRect(OpRect(rect.x, rect.y + rect.height - top_margin, rect.width, top_margin));
	m_item_details_group->SetVisibility(m_showdetails);
	m_drag_offset_y = rect.y;

	INT32 max_width = m_item_details_transferred_label->GetTextWidth();
	INT32 candidate = m_item_details_to_label->GetTextWidth();
	if( candidate > max_width ) max_width = candidate;
	candidate = m_item_details_size_label->GetTextWidth();
	if( candidate > max_width ) max_width = candidate;
	candidate = m_item_details_transferred_label->GetTextWidth();
	if( candidate > max_width ) max_width = candidate;

	if(m_show_extra_info)
	{
		candidate = m_item_details_connections_label->GetTextWidth();
		if( candidate > max_width ) max_width = candidate;
		candidate = m_item_details_activetransfers_label->GetTextWidth();
		if( candidate > max_width ) max_width = candidate;
	}

	m_item_details_group->SetChildRect(m_item_details_from_label, OpRect(4, 0, max_width, 20));
	m_item_details_group->SetChildRect(m_item_details_to_label, OpRect(4, 20, max_width, 20));
	m_item_details_group->SetChildRect(m_item_details_size_label, OpRect(4, 40, max_width, 20));
	m_item_details_group->SetChildRect(m_item_details_transferred_label, OpRect(4, 60, max_width, 20));

	if(m_show_extra_info)
	{
		m_item_details_group->SetChildRect(m_item_details_connections_label, OpRect(4, 80, max_width , 20));
		m_item_details_group->SetChildRect(m_item_details_activetransfers_label, OpRect(4, 100, max_width , 20));
	}
	m_item_details_group->SetChildRect(m_item_details_from_info_label, OpRect(max_width + 10, 0, rect.width - max_width - 1 , 20));
	m_item_details_group->SetChildRect(m_item_details_to_info_label, OpRect(max_width + 10, 20, rect.width - max_width - 1 , 20));
	m_item_details_group->SetChildRect(m_item_details_size_info_label, OpRect(max_width + 10, 40, rect.width - max_width - 1 , 20));
	m_item_details_group->SetChildRect(m_item_details_transferred_info_label, OpRect(max_width + 10, 60, rect.width - max_width - 1 , 20));

	if(m_show_extra_info)
	{
		m_item_details_group->SetChildRect(m_item_details_connections_info_label, OpRect(max_width + 10, 80, rect.width - max_width - 1 , 20));
		m_item_details_group->SetChildRect(m_item_details_activetransfers_info_label, OpRect(max_width + 10, 100, rect.width - max_width - 1 , 20));
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnChange(OpWidget *widget,
							  BOOL changed_by_mouse)
{
	if (widget == m_transfers_view)
	{
		TransferItemContainer* item = (TransferItemContainer*)((OpTreeView*)widget)->GetSelectedItem();
		UpdateDetails(item ? item->GetAssociatedItem() : NULL);
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnFocus(BOOL focus,
							 FOCUS_REASON reason)
{
	if (focus)
	{
		m_transfers_view->SetFocus(reason);
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnTransferProgress(TransferItemContainer* transferItem,
										OpTransferListener::TransferStatus status)
{
	PanelChanged();

	TransferItemContainer* item = (TransferItemContainer*)m_transfers_view->GetSelectedItem();
	if(!item)
	{
		m_item_details_from_info_label->SetText(UNI_L(""));
		m_item_details_to_info_label->SetText(UNI_L(""));
		m_item_details_size_info_label->SetText(UNI_L(""));
		m_item_details_transferred_info_label->SetText(UNI_L(""));
		m_item_details_connections_info_label->SetText(UNI_L(""));
		m_item_details_activetransfers_info_label->SetText(UNI_L(""));
		return;
	}

	if(item == transferItem)
		UpdateDetails(transferItem->GetAssociatedItem(), TRUE);
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnTransferAdded(TransferItemContainer* transferItem)
{
	PanelChanged();

	if (m_transfers_view->IsFocused())
		return;

	TransferItemContainer* item = (TransferItemContainer*)m_transfers_view->GetSelectedItem();

	if (item && item->GetStatus() == OpTransferListener::TRANSFER_PROGRESS)
	{
		return;
	}

	m_transfers_view->SetSelectedItem(m_transfers_view->GetItemByModelItem(transferItem));
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnTransferRemoved(TransferItemContainer* transferItem)
{
	PanelChanged();
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::ShowExtraInformation(BOOL show_extra_info)
{
	BOOL changed = (m_show_extra_info != show_extra_info);

	m_show_extra_info = show_extra_info;

	if(changed)
	{
		m_item_details_connections_label->SetVisibility(m_show_extra_info);
		m_item_details_activetransfers_label->SetVisibility(m_show_extra_info);

		m_item_details_connections_info_label->SetVisibility(m_show_extra_info);
		m_item_details_activetransfers_info_label->SetVisibility(m_show_extra_info);

		Relayout();
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::UpdateDetails(TransferItem* t_item,
								   BOOL onlysize)
{

	if(!t_item)	//clear the fields
	{
		m_item_details_from_info_label->SetText(UNI_L(""));
		m_item_details_to_info_label->SetText(UNI_L(""));
		m_item_details_size_info_label->SetText(UNI_L(""));
		m_item_details_transferred_info_label->SetText(UNI_L(""));
		m_item_details_connections_info_label->SetText(UNI_L(""));
		m_item_details_activetransfers_info_label->SetText(UNI_L(""));
		return;
	}

	if(!onlysize)
	{
		if(t_item->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
		{
			OpString filename;

			m_item_details_from_info_label->SetText(t_item->GetStorageFilename()->CStr());

			t_item->GetDownloadDirectory(filename);

			UINT32 idx = filename.FindLastOf(PATHSEPCHAR);

			if(idx + 1 != 0)
			{
				filename.Append(PATHSEP);
			}
			filename.Append(*(t_item->GetStorageFilename()));
			m_item_details_to_info_label->SetText(filename.CStr());

			ShowExtraInformation(TRUE);
		}
		else
		{
			OpString url_string;
			t_item->GetURL()->GetAttribute(URL::KUniName_Username_Password_Hidden, url_string);
			m_item_details_from_info_label->SetText(url_string.CStr());
			m_item_details_to_info_label->SetText(t_item->GetStorageFilename()->CStr());

			ShowExtraInformation(FALSE);
		}
	}
	if(t_item->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
	{
		OpString loc_conns;
		OpString loc_trans;
		OpString ui_text;
		OP_STATUS err;

		err = g_languageManager->GetString(Str::S_TRANSFERS_PANEL_CONNECTION_VERBOSE, ui_text);

		// Seeds: %d, Peers: %d (Total seeds/peers: %d/%d)

		if(OpStatus::IsSuccess(err))
		{
			loc_conns.AppendFormat(ui_text.CStr(),
				t_item->GetConnectionsWithCompleteFile(),
				t_item->GetConnectionsWithIncompleteFile(),
				t_item->GetTotalWithCompleteFile(),
				t_item->GetTotalDownloaders());
		}

#ifdef DEBUG_BT_TRANSFERPANEL

		BTDownload *download = (BTDownload *)g_Downloads->GetDownloadFromTransferItem(t_item);

		OpString debug_info;
		debug_info.AppendFormat(UNI_L(" (DEBUG: Unchoked: %d, Interested: %d, Interesting: %d)"),
//			download ? download->ClientConnectors()->GetCount() : 0,
//			g_Transfers->GetTotalCount(),
			download ? download->GetUploads()->GetTransferCount(upsUnchoked) : 0,
			download ? download->GetUploads()->GetTransferCount(upsInterested) : 0,
			download ? download->GetTransferCount(dtsCountInterestingPeers) : 0
			);

		loc_conns.Append(debug_info);
#endif
		m_item_details_connections_info_label->SetText(loc_conns.CStr());

		loc_trans.AppendFormat(UNI_L("%d / %d"),
			t_item->GetDownloadConnections(),
			t_item->GetUploadConnections());

		m_item_details_activetransfers_info_label->SetText(loc_trans.CStr());
	}

	OpString sizestring;
	OpString appendedsizestring;
	if (!sizestring.Reserve(256))
		return;

	OpFileLength downloaded_size = t_item->GetHaveSize();
	OpFileLength reported_size = t_item->GetSize();

	if(reported_size < downloaded_size)
		sizestring.Set("?");
	else
		StrFormatByteSize(sizestring, reported_size, SFBS_ABBRIVATEBYTES|SFBS_DETAILS);

	m_item_details_size_info_label->SetText(sizestring.CStr());
	if(t_item->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
	{
		StrFormatByteSize(sizestring, downloaded_size, SFBS_ABBRIVATEBYTES|SFBS_DETAILS);
		appendedsizestring.Set(sizestring);
		appendedsizestring.Append(" / ");
		StrFormatByteSize(sizestring, t_item->GetUploaded(), SFBS_ABBRIVATEBYTES|SFBS_DETAILS);
		appendedsizestring.Append(sizestring);
		m_item_details_transferred_info_label->SetText(appendedsizestring.CStr());
	}
	else
	{
		StrFormatByteSize(sizestring, downloaded_size, SFBS_ABBRIVATEBYTES|SFBS_DETAILS);
		m_item_details_transferred_info_label->SetText(sizestring.CStr());
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnKeyboardInputGained(OpInputContext* new_input_context,
										   OpInputContext* old_input_context,
										   FOCUS_REASON reason)
{
	OpInputContext::OnKeyboardInputGained(new_input_context, old_input_context, reason);
	g_desktop_transfer_manager->SetNewTransfersDone(FALSE);
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnNewTransfersDone()
{
	if (IsFocused(TRUE))
		g_desktop_transfer_manager->SetNewTransfersDone(FALSE);

	PanelChanged();
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::OnInputAction(OpInputAction* action)
{
	TransferItemContainer* item = (TransferItemContainer*)m_transfers_view->GetSelectedItem();
	TransferItem* OP_MEMORY_VAR t_item = item ? item->GetAssociatedItem() : NULL;

	switch (action->GetAction())
	{

	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch(child_action->GetAction())
			{

			case OpInputAction::ACTION_RESUME_TRANSFER:

				return EnableResumeTransfer(child_action, t_item, item);

			case OpInputAction::ACTION_STOP_TRANSFER:

				return EnableStopTransfer(child_action, item);

			case OpInputAction::ACTION_RESTART_TRANSFER:

				return EnableRestartTransfer(child_action, t_item);

			case OpInputAction::ACTION_DELETE:
			case OpInputAction::ACTION_COPY_TRANSFER_INFO:

				child_action->SetEnabled(item != NULL);
				return TRUE;

			case OpInputAction::ACTION_REMOVE_ALL_FINISHED_TRANSFERS:

				return EnableRemoveAllFinishedTransfers(child_action);

			case OpInputAction::ACTION_SHOW_TRANSFERPANEL_DETAILS:
			case OpInputAction::ACTION_HIDE_TRANSFERPANEL_DETAILS:

				child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SHOW_TRANSFERPANEL_DETAILS,
														g_pcui->GetIntegerPref(PrefsCollectionUI::TransWinShowDetails));
				return TRUE;

			case OpInputAction::ACTION_SHOW_NEW_TRANSFERS_ON_TOP:
			case OpInputAction::ACTION_SHOW_NEW_TRANSFERS_ON_BOTTOM:

				child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SHOW_NEW_TRANSFERS_ON_TOP,
														g_pcui->GetIntegerPref(PrefsCollectionUI::TransferItemsAddedOnTop));
				return TRUE;

			default:

				return FALSE;
			}
			break;
		}


	case OpInputAction::ACTION_EXECUTE_TRANSFERITEM:
		{
			ExecuteTransferItem(t_item);
			break; // This will return false - why?
		}


	case OpInputAction::ACTION_STOP_TRANSFER:
	case OpInputAction::ACTION_RESUME_TRANSFER:
	case OpInputAction::ACTION_RESTART_TRANSFER:
	case OpInputAction::ACTION_DELETE:

		return DoAllSelected(action->GetAction());

	case OpInputAction::ACTION_REMOVE_ALL_FINISHED_TRANSFERS:

		return RemoveAllFinished();

	case OpInputAction::ACTION_OPEN_TRANSFER:

		if (item)
			item->Open(FALSE);
		return TRUE;

	case OpInputAction::ACTION_OPEN_TRANSFER_FOLDER:

		if (item)
			item->Open(TRUE);
		return TRUE;

	case OpInputAction::ACTION_COPY_TRANSFER_INFO:

		return CopyTransferInfo(t_item);

	case OpInputAction::ACTION_SHOW_CONTEXT_MENU:

		ShowContextMenu(GetBounds().Center(),TRUE,TRUE);
		return TRUE;

	case OpInputAction::ACTION_SHOW_TRANSFERPANEL_DETAILS:

		ShowDetails(TRUE);
		return FALSE;		//drop to application for saving of preferences

	case OpInputAction::ACTION_HIDE_TRANSFERPANEL_DETAILS:

		ShowDetails(FALSE);
		return FALSE;		//drop to application for saving of preferences

	}
	return FALSE;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::EnableResumeTransfer(OpInputAction * child_action,
										  TransferItem * t_item,
										  TransferItemContainer * item)
{
	BOOL isfinishedorrunning;
	BOOL isresumable;

	if (!t_item)
	{
		child_action->SetEnabled(FALSE);
		return TRUE;
	}

	if(t_item->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
	{
		isresumable = (item->GetStatus() != OpTransferListener::TRANSFER_PROGRESS &&
					   item->GetStatus() != OpTransferListener::TRANSFER_CHECKING_FILES &&
					   item->GetStatus() != OpTransferListener::TRANSFER_SHARING_FILES);

		isfinishedorrunning = FALSE;
	}
	else
	{
		isresumable = (URL_Resumable_Status) t_item->GetURL()->GetAttribute(URL::KResumeSupported) != Not_Resumable;

		isfinishedorrunning = (item->GetStatus() == OpTransferListener::TRANSFER_DONE ||
							   item->GetStatus() == OpTransferListener::TRANSFER_PROGRESS);
	}

	child_action->SetEnabled(isresumable && !isfinishedorrunning);

	return TRUE;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::EnableStopTransfer(OpInputAction * child_action,
										TransferItemContainer * item)
{
	child_action->SetEnabled(item &&
							 (item->GetStatus() == OpTransferListener::TRANSFER_PROGRESS ||
							  item->GetStatus() == OpTransferListener::TRANSFER_CHECKING_FILES ||
							  item->GetStatus() == OpTransferListener::TRANSFER_SHARING_FILES));
	return TRUE;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::EnableRestartTransfer(OpInputAction * child_action,
										   TransferItem * t_item)
{
	child_action->SetEnabled(t_item &&
							 (t_item->GetType() != TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD));
	return TRUE;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::EnableRemoveAllFinishedTransfers(OpInputAction * child_action)
{
	for( INT32 i=0; i<m_transfers_view->GetItemCount(); i++)
	{
		TransferItemContainer* citem = (TransferItemContainer*)m_transfers_view->GetItemByPosition(i);
		if( citem && citem->GetStatus() == OpTransferListener::TRANSFER_DONE )
		{
			child_action->SetEnabled(TRUE);
			return TRUE;
		}
	}
	child_action->SetEnabled(FALSE);
	return TRUE;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::ExecuteTransferItem(TransferItem * t_item)
{
	if(!t_item)
		return;

	OpString filename;

	if(t_item->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
	{
		t_item->GetDownloadDirectory(filename);

		UINT32 idx = filename.FindLastOf(PATHSEPCHAR);

		if(idx + 1 != 0)
		{
			filename.Append(PATHSEP);
		}
		filename.Append(*(t_item->GetStorageFilename()));
	}
	else
	{
		filename.Set(*(t_item->GetStorageFilename()));
	}
	if(!filename.IsEmpty())
	{
#ifdef MSWIN
		Execute(filename.CStr(), NULL);
#else
		OpString handler;
		OpString content_type;

		URL* url = t_item->GetURL();

		if(url)
		{
			content_type.Set(url->GetAttribute(URL::KMIME_Type));
		}

		g_op_system_info->GetFileHandler(&filename, content_type, handler);
		static_cast<DesktopOpSystemInfo*>(g_op_system_info)->OpenFileInApplication(
				handler.CStr(), filename.CStr());
#endif
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::CopyTransferInfo(TransferItem * t_item)
{
	if (!t_item)
		return TRUE;

	OpString str, string;

	g_languageManager->GetString(Str::DI_IDL_SOURCE, string);
	str.Append(string);
	str.Append(": ");

	OpString url_string;
	t_item->GetURL()->GetAttribute(URL::KUniName_Username_Password_Hidden, url_string);

	if(t_item->GetType() == TransferItem::TRANSFERTYPE_CHAT_UPLOAD ||
	   t_item->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
		str.Append(t_item->GetStorageFilename()->CStr());
	else
		str.Append(url_string);

	str.Append(NEWLINE);

	g_languageManager->GetString(Str::DI_IDL_DESTINATION, string);
	str.Append(string);
	str.Append(": ");

	if(t_item->GetType() == TransferItem::TRANSFERTYPE_CHAT_UPLOAD ||
	   t_item->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
		str.Append(url_string);
	else
		str.Append(t_item->GetStorageFilename()->CStr());

	str.Append(NEWLINE);

	g_languageManager->GetString(Str::DI_IDL_SIZE, string);
	str.Append(string);
	str.Append(": ");

	OpString sizestring;
	if (sizestring.Reserve(256))
	{
		StrFormatByteSize(sizestring, t_item->GetSize(), SFBS_ABBRIVATEBYTES|SFBS_DETAILS);
		str.Append(sizestring.CStr());
		str.Append(NEWLINE);

		g_languageManager->GetString(Str::DI_IDL_TRANSFERED, string);
		str.Append(string);
		str.Append(": ");

		StrFormatByteSize(sizestring, t_item->GetHaveSize(), SFBS_ABBRIVATEBYTES|SFBS_DETAILS);
		str.Append(sizestring.CStr());
	}
	str.Append(NEWLINE);

	g_desktop_clipboard_manager->PlaceText(str.CStr(), g_application->GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
	g_desktop_clipboard_manager->PlaceText(str.CStr(), g_application->GetClipboardToken(), true);
#endif
	return TRUE;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnMouseEvent(OpWidget *widget,
								  INT32 pos,
								  INT32 x,
								  INT32 y,
								  MouseButton button,
								  BOOL down,
								  UINT8 nclicks)
{
	if (down)
	{
		if (nclicks == 2 && button == MOUSE_BUTTON_1)
		{
			OpInputAction action(OpInputAction::ACTION_EXECUTE_TRANSFERITEM);
			OnInputAction(&action);
		}
	}
	else if (nclicks == 1 && button == MOUSE_BUTTON_2)
	{
		if( widget == m_transfers_view )
		{
			ShowContextMenu(OpPoint(x+widget->GetRect().x,y+widget->GetRect().y),FALSE,FALSE);
		}
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::ShowContextMenu(const OpPoint &point,
									 BOOL center,
									 BOOL use_keyboard_context)
{
	TransferItemContainer* item = (TransferItemContainer*)m_transfers_view->GetSelectedItem();
	if(!item)
		return TRUE;

	TransferItem* titem = item->GetAssociatedItem();
	uni_char* filename = NULL;
	OpString tmpfile;

	if(titem->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
	{
		if(item->GetStatus() == OpTransferListener::TRANSFER_DONE ||
			item->GetStatus() == OpTransferListener::TRANSFER_ABORTED)
		{
			titem->GetDownloadDirectory(tmpfile);

			UINT32 idx = tmpfile.FindLastOf(PATHSEPCHAR);

			if(idx + 1 != 0)
			{
				tmpfile.Append(PATHSEP);
			}
			tmpfile.Append(*(titem->GetStorageFilename()));
			filename = tmpfile.CStr();
		}
		else
		{
			filename = titem->GetStorageFilename()->CStr();
		}
	}
	else
	{
		filename = titem->GetStorageFilename()->CStr();
	}

	OpString file_path;
	file_path.Set(filename);

	OpString file_url_str;
	g_url_api->ResolveUrlNameL(file_path, file_url_str);

	DesktopMenuContext * menu_context  = NULL;
	TransferManagerDownloadCallback * t_file_download_callback = NULL;
	menu_context = OP_NEW(DesktopMenuContext, ());
	if (menu_context)
	{
		URL file_url = g_url_api->GetURL(file_url_str.CStr());
		t_file_download_callback = OP_NEW(TransferManagerDownloadCallback, (NULL, file_url, NULL, ViewActionFlag()));
		if (t_file_download_callback)
		{
#ifdef WIC_CAP_URLINFO_MIMETYPE_OVERRIDE 
			URLInformation * t_url_info = titem->CreateURLInformation();
			t_file_download_callback->SetMIMETypeOverride(t_url_info->MIMEType());
			t_url_info->URL_Information_Done();
#endif // WIC_CAP_URLINFO_MIMETYPE_OVERRIDE
			menu_context->Init(t_file_download_callback); 

			OpRect rect;
			const uni_char * menu_name = NULL;
			ResolveContextMenu(point, menu_context, menu_name, rect);
			InvokeContextMenuAction(point, menu_context, menu_name, rect);
		}
		else
			OP_DELETE(menu_context);
	}
	return TRUE;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnDragStart(OpWidget* widget,
								 INT32 pos,
								 INT32 x,
								 INT32 y)
{
	y += m_drag_offset_y;

	DesktopDragObject* drag_object = m_transfers_view->GetDragObject(DRAG_TYPE_TRANSFER, x, y);

	if (drag_object)
	{
		OpINT32Vector selected;
		m_transfers_view->GetSelectedItems(selected, FALSE);
		for (UINT i = 0; i < selected.GetCount(); i++)
		{
			int id = selected.Get(i);
			TransferItemContainer* item = static_cast<TransferItemContainer*>(
					g_desktop_transfer_manager->GetItemByPosition(id));
			if (item)
			{
				OpString filename;
				OpAutoPtr<OpString> url(OP_NEW(OpString, ()));
				if (OpStatus::IsSuccess(item->MakeFullPath(filename)) &&
					OpStatus::IsSuccess(url->Set(filename.CStr())))
				{
					// Escape?
					drag_object->AddURL(url.release());
				}
			}
		}
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

void TransfersPanel::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	if( KioskManager::GetInstance()->GetNoDownload() )
	{
		drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
		return;
	}
	if (drag_object->GetURL() && drag_object->GetType() != OpTypedObject::DRAG_TYPE_TRANSFER)
	{
		drag_object->SetDesktopDropType(DROP_COPY);
	}
	else
	{
		drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::OnDragDrop(OpWidget* widget,
								OpDragObject* op_drag_object,
								INT32 pos,
								INT32 x,
								INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if( KioskManager::GetInstance()->GetNoDownload() )
	{
		return;
	}

	if (drag_object->GetURL())
	{
		// start download to download directory

		// We could also check on url-type here, if it was a directory it could be traversed
		// and the whole directory tree could be put into the transferqueue. We should have
		// a better queueing system before this is added though. (Max simultaneous transfers,
		// with a stack on the side.)

		OpTransferItem* item;
		OpString filename;
		URL durl = g_url_api->GetURL(drag_object->GetURL());

		OpString tmp_storage;
		const OpStringC downloaddirectory = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DOWNLOAD_FOLDER, tmp_storage);

		filename.Set(downloaddirectory);

		OpString tmp;
		TRAPD(op_err, durl.GetAttribute(URL::KSuggestedFileName_L, tmp, TRUE));
		filename.Append(tmp);

		durl.LoadToFile(filename.CStr());

        // need to set the timestamp, this is used for expiry in list when read from rescuefile
		time_t loaded = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);
		durl.SetAttribute(URL::KVLocalTimeLoaded, &loaded);

		if(OpStatus::IsError(((TransferManager*)g_transferManager)->AddTransferItem(durl, filename.CStr())))
		{
			return;
		}

		((TransferManager*)g_transferManager)->GetTransferItem(&item, drag_object->GetURL());

		item->Continue();
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::DoAllSelected(OpInputAction::Action action)
{
	OpINT32Vector items;
	BOOL ret = FALSE;
	int selected_items = m_transfers_view->GetSelectedItems(items, FALSE);

	if (selected_items == 0)
	{
		return FALSE;
	}

	for (int i = selected_items - 1; i >= 0; i--)
	{
		TransferItemContainer* citem = (TransferItemContainer*)m_transfers_view->GetItemByPosition(items.Get(i));
		TransferItem* item = citem->GetAssociatedItem();

		if (!item)
			continue;


		switch (action)
		{
		case OpInputAction::ACTION_RESUME_TRANSFER:
			{
				item->Continue();
				item->UpdateStats();
				ret = TRUE;
			}
			break;
		case OpInputAction::ACTION_STOP_TRANSFER:
			{
				if (citem->GetStatus() == OpTransferListener::TRANSFER_PROGRESS ||
						citem->GetStatus() == OpTransferListener::TRANSFER_CHECKING_FILES ||
						citem->GetStatus() == OpTransferListener::TRANSFER_SHARING_FILES)
				{
					item->Stop();
					ret = TRUE;
				}
			}
			break;
		case OpInputAction::ACTION_RESTART_TRANSFER:
			{
				item->Clear();
				item->GetURL()->ForceStatus(URL_UNLOADED);
				item->Continue();
				item->UpdateStats();
				ret = TRUE;
			}
			break;
		case OpInputAction::ACTION_DELETE:
			{
				((TransferManager*)g_transferManager)->ReleaseTransferItem((OpTransferItem*)item);
				g_desktop_transfer_manager->OnTransferItemRemoved((OpTransferItem*)item);
				ret = TRUE;
			}
			break;
		default:
			;

		}
	}
	return ret;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::RemoveAllFinished()
{
	OpINT32Vector list;
	INT32 i;

	for( i=0; i<m_transfers_view->GetItemCount(); i++)
	{
		TransferItemContainer* citem = (TransferItemContainer*)m_transfers_view->GetItemByPosition(i);
		if( citem && citem->GetStatus() == OpTransferListener::TRANSFER_DONE )
		{
			list.Add(i);
		}
	}

	for( i=list.GetCount() - 1; i >= 0; i--)
	{
		TransferItemContainer* citem = (TransferItemContainer*)m_transfers_view->GetItemByPosition(list.Get(i));
		if( citem && citem->GetAssociatedItem() )
		{
			TransferItem* item = citem->GetAssociatedItem();
			((TransferManager*)g_transferManager)->ReleaseTransferItem((OpTransferItem*)item);
			g_desktop_transfer_manager->OnTransferItemRemoved((OpTransferItem*)item);
		}
	}

	return TRUE;
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::GetTimeRemainingText(OpString& text)
{
	text.Empty();
	UINT32 remaining_transfers = 0;

	UINT32 maxtime_left = g_desktop_transfer_manager->GetMaxTimeRemaining(&remaining_transfers);

	if(maxtime_left == 0)
	{
		return;
	}

	UINT32 days = maxtime_left/(60*60*24);
	maxtime_left -= days*60*60*24;
	UINT32 hours = maxtime_left/(60*60);
	maxtime_left -= hours*60*60;
	UINT32 minutes = maxtime_left/60;
	maxtime_left -= minutes*60;

	if (days)
	{
		text.AppendFormat(UNI_L("%d %d"), days, hours);
	}
	else if (hours)
	{
		text.AppendFormat(UNI_L("%d:%02d"), hours, minutes);
	}
	else
	{
		text.AppendFormat(UNI_L("%02d:%02d"), minutes, maxtime_left);
	}

	if (remaining_transfers >= 2)
	{
		text.AppendFormat(UNI_L(" (%d)"), remaining_transfers);
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::GetWindowTitle(OpString& text)
{
	OpString remaining;

	g_languageManager->GetString(Str::MI_IDM_DownloadWindow, text);

	GetTimeRemainingText(remaining);

	if(remaining.HasContent())
	{
		text.Append(" ");
		text.Append(remaining.CStr());
	}
}


/***********************************************************************************
**
**
**
***********************************************************************************/
void TransfersPanel::ShowDetails(BOOL showdetails)
{
	m_showdetails = showdetails;
	Relayout();
}


/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL TransfersPanel::WidgetListener::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	g_application->GetMenuHandler()->ShowPopupMenu("Readonly Edit Widget Popup Menu", PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
	return TRUE;
}
