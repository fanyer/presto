/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LINK_STYLE_DIALOG_H
#define LINK_STYLE_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "adjunct/desktop_pi/DesktopColorChooser.h"

class LinkStyleDialog : public Dialog, public OpColorChooserListener
{
	Window*					m_current_window;
	public:

		Type				GetType()				{return DIALOG_TYPE_LINK_STYLE;}
		const char*			GetWindowName()			{return "Link Style Dialog";}
		const char*			GetHelpAnchor()			{return "fonts.html";}

		void				Init(DesktopWindow* parent_window);
		void				OnInit();

		void				OnColorSelected(COLORREF color);
		
		UINT32				OnOk();
		BOOL				OnInputAction(OpInputAction* action);		

	private:
		BOOL				m_choose_visited;
};


#endif //this file
