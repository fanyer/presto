/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 */

#ifndef INDEX_VIEW_PROPERTIES_CONTROLLER_H
#define INDEX_VIEW_PROPERTIES_CONTROLLER_H

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"

class MailDesktopWindow;

class IndexViewPropertiesController : public PopupDialogContext
{
public:
	IndexViewPropertiesController(OpWidget* anchor_widget, index_gid_t index_id, MailDesktopWindow* mail_window);
	virtual ~IndexViewPropertiesController();

private:
	// DialogContext
	virtual void			InitL();

			OP_STATUS		InitOptions();
			OP_STATUS		InitShowFlags();

							// Delegate functions to reinit the tree model when a property is changed
			void			ReinitTreeModel(bool);
			void			ReinitTreeModelInt(int) { ReinitTreeModel(true); }
			void			UpdateShowFlags(bool);

	index_gid_t					m_index_id;
	MailDesktopWindow*			m_mail_window;
	Index*						m_index;
	OpProperty<bool>			m_custom_sorting;
	OpProperty<bool>			m_sort_ascending;
	OpProperty<bool>			m_threaded;
	OpProperty<int>				m_sorting;
	OpProperty<int>				m_grouping;
	OpProperty<bool>			m_show_flags[IndexTypes::MODEL_FLAG_LAST];
};


#endif // M2_SUPPORT
#endif // INDEX_VIEW_PROPERTIES_CONTROLLER_H
