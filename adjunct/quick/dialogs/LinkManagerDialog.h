/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef LINK_MANAGER_DIALOG_H
#define LINK_MANAGER_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class OpLinksView;

/***********************************************************************************
**
**	LinkManagerDialog
**
***********************************************************************************/

class LinkManagerDialog : public Dialog
{
	public:
			
								LinkManagerDialog() : m_links_view(NULL) {};
		virtual					~LinkManagerDialog();

		DialogType				GetDialogType()			{return TYPE_CLOSE;}
		Type					GetType()				{return DIALOG_TYPE_LINK_MANAGER;}
		const char*				GetWindowName()			{return "Link Manager Dialog";}
		BOOL					GetModality()			{return FALSE;}
		const char*				GetHelpAnchor()			{return "panels.html#links";}

		virtual void			OnInit();

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual void			OnClick(OpWidget *widget, UINT32 id = 0);

	private:

		OpLinksView*			m_links_view;
};

#endif //LINK_MANAGER_DIALOG_H
