/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#ifndef MAIL_PANEL_H
#define MAIL_PANEL_H

#if defined M2_SUPPORT
#include "adjunct/desktop_pi/desktop_file_chooser.h"

#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2_ui/models/accesstreeview.h"
#include "adjunct/quick/panels/HotlistPanel.h"
#include "adjunct/quick_toolkit/widgets/OpAccordion.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/hardcore/timer/optimer.h"

/***********************************************************************************
**
**	MailPanel
**
***********************************************************************************/

class OpProgressBar;

class MailPanel
  : public HotlistPanel
  , public CategoryListener
  , public EngineListener
  , public ProgressInfoListener
  , public MailNotificationListener
  , public DesktopFileChooserListener
  , public OpAccordion::OpAccordionListener
{
	public:

								MailPanel();

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Mail";}
		virtual BOOL			GetPanelAttention() { return m_needs_attention; }

		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);

		virtual	void			OnDeleted();

		/** Get list of categories from indexer and populate the mail panel
		 * @param reset - whether it should reset to the default list 
		 */
		OP_STATUS				PopulateCategories(BOOL reset = FALSE);

		virtual Type			GetType() {return PANEL_TYPE_MAIL;}

		// OpWidgetListener
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { m_accordion->OnDragMove(widget, drag_object, pos, x, y); }
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { m_accordion->OnDragDrop(widget, drag_object, pos, x, y); }
		virtual void			OnDragLeave(OpWidget* widget, OpDragObject* drag_object) { m_accordion->OnDragLeave(widget, drag_object); };

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Mail Panel";}

		// == CategoryListener =================
		virtual OP_STATUS		CategoryAdded(UINT32 index_id) { return AddCategory(index_id); }
		virtual OP_STATUS		CategoryRemoved(UINT32 index_id);
		virtual OP_STATUS		CategoryUnreadChanged(UINT32 index_id, UINT32 unread_messages);
		virtual OP_STATUS		CategoryNameChanged(UINT32 index_id);
		virtual OP_STATUS		CategoryVisibleChanged(UINT32 index_id, BOOL visible);

		// == EngineListener =================
		virtual void OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE) {}
		virtual void OnImporterFinished(const Importer* importer, const OpStringC& infoMsg) {}
		virtual void OnIndexChanged(UINT32 index_id) {}
		virtual void OnActiveAccountChanged() { PopulateCategories(); }
		virtual void OnReindexingProgressChanged(INT32 progress, INT32 total) {}

		// == ProgressInfoListener ==========================

		virtual void			OnProgressChanged(const ProgressInfo& progress);
		virtual void			OnSubProgressChanged(const ProgressInfo& progress);
		virtual void			OnStatusChanged(const ProgressInfo& progress);

		// == MailNotificationListener ======================

		virtual void			NeedNewMessagesNotification(Account* account, unsigned count);
		virtual void			NeedNoMessagesNotification(Account* account) {}
		virtual void			NeedSoundNotification(const OpStringC& sound_path) {}
		virtual void			OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active);

		// == OpTimerListener ==========================

		virtual void			OnTimeOut(OpTimer* timer);

		// == FileChooserListener ==========================
		
		virtual void			OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

		// == OpAccordionListener ==========================

		virtual void			OnItemExpanded( UINT32 id, BOOL expanded);
		virtual void			OnItemMoved(UINT32 id, UINT32 new_position);

		void					SetSelectedIndex(index_gid_t index_id);
		OP_STATUS				ExportMailIndex(UINT32 index_id);

		OP_STATUS				GetUnreadBadgeWidth(INT32& w);
	private:

		OpProgressBar*				m_status_bar_main;
		OpButton*					m_status_bar_stop;
		OpProgressBar*				m_status_bar_sub;
		BOOL						m_status_timer_started;
		OpTimer						m_status_timer;
		OpAccordion*				m_accordion;
		OpVector<AccessTreeView>	m_access_views;		// added as children in the accordion and deleted there
		BOOL						m_needs_attention;
		DesktopFileChooser*			m_chooser;
		DesktopFileChooserRequest	m_request;
		index_gid_t					m_export_index;
		INT32						m_unread_badge_width;

		OP_STATUS				AddCategory(UINT32 index_id);

		OP_STATUS				InitAccessView(OpTreeView* access_view, IndexTypes::Id category_id);

		const char* GetNewIconForAccount(UINT32 index_id);

};

#endif //M2_SUPPORT

#endif // MAIL_PANEL_H
