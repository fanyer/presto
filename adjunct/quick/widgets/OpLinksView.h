/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 * 
 *  Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 * 
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef OP_LINKS_VIEW_H
#define OP_LINKS_VIEW_H

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "modules/widgets/OpWidget.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

class LinksModel;

/***********************************************************************************
**
**	OpLinksView
**
***********************************************************************************/

class OpLinksView : public OpWidget, public DesktopWindowSpy, public DesktopFileChooserListener
{
	private:
		struct LinkData
		{
			OpString address;
			OpString title;
		};

	protected:
		OpLinksView();

	public:

		static OP_STATUS Construct(OpLinksView** obj);

		void					SetDetailed(BOOL detailed, BOOL force = FALSE);

		virtual void			OnResize(INT32* new_w, INT32* new_h) {m_links_view->SetRect(GetBounds());}
		virtual void			OnShow(BOOL show);
		virtual void			OnAdded() {UpdateTargetDesktopWindow(); UpdateLinks(FALSE);}

		BOOL					IsSingleClick();

		BOOL					SetPanelLocked(BOOL locked);

		virtual Type			GetType() {return WIDGET_TYPE_LINKS_VIEW;}
		BOOL					ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context);

		// subclassing OpWidget

		virtual void			SetAction(OpInputAction* action) {m_links_view->SetAction(action); }
		virtual void			OnSettingsChanged(DesktopSettings* settings);
		virtual void			OnDeleted();

		// DesktopFileChooserListener
		virtual void			OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

		// OpWidgetListener

	 	virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnTimer() {UpdateLinks(FALSE);}

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Links Widget";}

		// DesktopWindowSpy hooks

		virtual void			OnTargetDesktopWindowChanged(DesktopWindow* target_window) {UpdateLinks(TRUE);}
		virtual void			OnPageStartLoading(OpWindowCommander* commander) {UpdateLinks(TRUE);}
		virtual void			OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status) {UpdateLinks(FALSE);}

		OpTreeView*				GetTree() const { return m_links_view; }

	private:
		void					DoAllSelected(OpInputAction::Action action);
		void					UpdateLinks(BOOL restart);

	private:
		DesktopFileChooser*		m_chooser;
		DesktopFileChooserRequest m_request;
		LinksModel*				m_links_model;
		OpTreeView*				m_links_view;
		BOOL					m_locked;
		BOOL					m_needs_update;
		BOOL					m_detailed;
		BOOL					m_private_mode;
};

#endif // OP_LINKS_VIEW_H
