/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef DESKTOP_TAB_H
#define DESKTOP_TAB_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"

class OpFindTextBar;

class DesktopTab
	: public DesktopWindow
	, public DesktopFileChooserListener
{
	public:
		DesktopTab();
		virtual ~DesktopTab();

		// From DesktopFileChooserListener
		virtual void				OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

		// From DesktopWindow - this function should return a non-NULL value
		virtual OpWindowCommander*	GetWindowCommander() = 0;

		void						ShowFindTextBar(BOOL show = TRUE);

		// WidgetListener
		virtual BOOL				OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

	protected:
		/** Save the current document in this window
		  * @param save_focused_frame Whether to save just the focused frame or the whole document
		  */
		OP_STATUS					SaveDocument(BOOL save_focused_frame);

		/** Get a file chooser object
		  * @param chooser Where to position the chooser
		  */
		OP_STATUS					GetChooser(DesktopFileChooser*& chooser);

		/** Get the find text bar
		  * @return OpFindTextBar*
		  */
		OpFindTextBar*				GetFindTextBar() { return m_search_bar; }

		DesktopFileChooserRequest	m_request;
	private:
		BOOL						m_save_focused_frame;
#ifdef DEBUG_ENABLE_OPASSERT
		int							m_waiting_for_filechooser;
#endif
		DesktopFileChooser*			m_chooser;
		static UINT32				s_default_filter;
		OpFindTextBar*				m_search_bar;
};

#endif // DESKTOP_TAB_H
