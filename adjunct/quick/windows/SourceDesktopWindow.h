/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef SOURCEDESKTOPWINDOW_H
#define SOURCEDESKTOPWINDOW_H


#include "adjunct/quick/windows/DesktopTab.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpMultiEdit.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

class SourceDesktopWindow : public DesktopTab
{
	public:
			
							SourceDesktopWindow(OpWorkspace* parent_workspace, URL *url, const uni_char *title, unsigned long window_id, BOOL frame_source);
							virtual				~SourceDesktopWindow() {}

		virtual void		OnLayout();
		virtual BOOL 		OnInputAction(OpInputAction* action);

		OpWindowCommander*	GetWindowCommander() { return NULL; }

		void 				OnChange(OpWidget *widget, BOOL changed_by_mouse);

		// OpTypedObject
		virtual Type		GetType() { return WINDOW_TYPE_SOURCE; }

		// DesktopWindow
		virtual const char*		GetWindowName() {return "Source Window";}

		// DesktopTab
		virtual void		OnSessionReadL(const OpSessionWindow* session_window) {}
		virtual void		OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown) {}
		virtual OP_STATUS	AddToSession(OpSession* session, INT32 parent_id, BOOL shutdown, BOOL extra_info = FALSE) { return OpStatus::OK; }


private:
		OpMultilineEdit		*m_source_editfield;	// Giant edit field that holds the source of the document
		URL                 *m_url;					// URL object we are showing the source of
		OpString            m_source;				// OpString holding the original copy of the source 
		OpToolbar			*m_toolbar;				// Toolbar
		BOOL				m_edited;				// Flag to say if the source has been edited
		OpString 			m_filename;				// Cache filename on disk on the source we are viewing
		OpString			m_save_filename;		// Name of the file on the sever (used if saving the source to disk)
		unsigned long 		m_window_id;			// Window ID of the original page we are viewing the source of
		OpString8			m_charset;				// Encoding of the source file

		void SetTitle(const uni_char *title);
		
		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);
};

#endif //SOURCEDESKTOPWINDOW_H
