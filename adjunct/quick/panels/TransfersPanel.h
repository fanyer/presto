/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef TRANSFERS_PANEL_H
#define TRANSFERS_PANEL_H

#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/panels/HotlistPanel.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

#include "modules/widgets/OpWidget.h"

class OpLabel;

/***********************************************************************************
 **
 **	TransfersPanel
 **
 ***********************************************************************************/

class TransfersPanel :
	public HotlistPanel,
	public DesktopTransferManager::TransferListener
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	   Constructor
	 */
	TransfersPanel();

	/**
       @return
    */
	virtual OP_STATUS Init();

	/**
       @param point                -
       @param center               -
	   @param use_keyboard_context -
	   @return
    */
	BOOL ShowContextMenu(const OpPoint& point,
						 BOOL center,
						 BOOL use_keyboard_context);

	/**
       @param showdetails -
    */
	void ShowDetails(BOOL showdetails);

	/**
       @param column -
    */
	void UpdateColumnHeaders(int column);

	/**
       @param text -
    */
	void GetWindowTitle(OpString& text);

	/**
       @param text -
    */
	void GetTimeRemainingText(OpString& text);


	// == OpTypedObject ======================

	virtual Type GetType() { return PANEL_TYPE_TRANSFERS; }


	// == HotlistPanel ======================

	virtual void OnLayoutPanel(OpRect& rect);

	virtual BOOL GetPanelAttention();

	virtual void GetPanelText(OpString& text,
							  Hotlist::PanelTextType text_type);

	virtual const char*	GetPanelImage() {return "Panel Transfers";}

	virtual void OnFullModeChanged(BOOL full_mode);


	// == OpWidgetListener ======================

	virtual void OnMouseEvent(OpWidget *widget,
							  INT32 pos,
							  INT32 x,
							  INT32 y,
							  MouseButton button,
							  BOOL down,
							  UINT8 nclicks);

	virtual void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);

	virtual void OnDragStart(OpWidget* widget,
							 INT32 pos,
							 INT32 x,
							 INT32 y);

	virtual void OnDragDrop(OpWidget* widget,
							OpDragObject* drag_object,
							INT32 pos,
							INT32 x,
							INT32 y);

	virtual void OnChange(OpWidget *widget,
						  BOOL changed_by_mouse);

	virtual	void			OnDeleted();

	// == OpTransfersContainerListener ======================

	void OnTransferProgress(TransferItemContainer* transferItem,
							OpTransferListener::TransferStatus status);

	void OnTransferAdded(TransferItemContainer* transferItem);

	void OnTransferRemoved(TransferItemContainer* transferItem);

	void OnNewTransfersDone();


	// == OpInputContext ======================

	virtual BOOL OnInputAction(OpInputAction* action);

	virtual void OnKeyboardInputGained(OpInputContext* new_input_context,
									   OpInputContext* old_input_context,
									   FOCUS_REASON reason);

	virtual const char*	GetInputContextName() {return "Transfers Panel";}

	virtual void OnFocus(BOOL focus,
						 FOCUS_REASON reason);

private:

	class WidgetListener:public OpWidgetListener
	{
		// OpWidgetListener
		void	OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks){ }
		void	OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y) { }
		void	OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { }
		void	OnDragLeave(OpWidget* widget, OpDragObject* drag_object) { }
		void	OnDropdownMenu(OpWidget *widget, BOOL show) { }
		void 	OnChange(OpWidget *widget, BOOL changed_by_mouse) { }
		BOOL 	OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);
	};

//  -------------------------
//  Private member functions:
//  -------------------------

	void UpdateDetails(TransferItem* t_item,
					   BOOL onlysize = FALSE);

	BOOL DoAllSelected(OpInputAction::Action action);

	void ShowExtraInformation(BOOL show_extra_info);

	OpStringC GetTitle();

	BOOL EnableResumeTransfer(OpInputAction * child_action,
							  TransferItem * t_item,
							  TransferItemContainer * item);

	BOOL EnableStopTransfer(OpInputAction * child_action,
							TransferItemContainer * item);

	BOOL EnableRestartTransfer(OpInputAction * child_action,
							   TransferItem * t_item);

	BOOL EnableRemoveAllFinishedTransfers(OpInputAction * child_action);

	void ExecuteTransferItem(TransferItem * t_item);

	BOOL CopyTransferInfo(TransferItem * t_item);

	BOOL RemoveAllFinished();

//  -------------------------
//  Private member variables:
//  -------------------------

	int				m_valuemaxtimeleft;
	int				m_drag_offset_y;

	OpTreeView*		m_transfers_view;

	OpGroup*		m_item_details_group;

	OpLabel*		m_item_details_from_label;
	OpLabel*		m_item_details_to_label;
	OpLabel*		m_item_details_size_label;
	OpLabel*		m_item_details_transferred_label;
	OpLabel*		m_item_details_connections_label;
	OpLabel*		m_item_details_activetransfers_label;

	OpEdit*			m_item_details_from_info_label;
	OpEdit*			m_item_details_to_info_label;
	OpEdit*			m_item_details_size_info_label;
	OpEdit*			m_item_details_transferred_info_label;
	OpEdit*			m_item_details_connections_info_label;
	OpEdit*			m_item_details_activetransfers_info_label;

	TransferItem*	m_selectedtransferitem;

	BOOL			m_showdetails;
	BOOL			m_show_extra_info;	
	WidgetListener	m_widgetlistener;

};

// ---------------------------------------------------------------------------------------------------------------


#endif // TRANSFERS_PANEL_H
