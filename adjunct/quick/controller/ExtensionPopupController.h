/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#ifndef OP_EXTENSIONPOPUPCONTROLLER_H
#define OP_EXTENSIONPOPUPCONTROLLER_H

#include "adjunct/quick_toolkit/contexts/CloseDialogContext.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"

class OpGadget;
class OpPageListener;
class OpWidget;
class OpWindowCommander;
class QuickBrowserView;

 /**
  * Controller for dialog, which next to OpRichMenuWindow with STYLE_EXTENSION_POPUP 
  * provides possibility to display extension popup comming from toolbar button
  * This controller displays popup with is not auto hided when losing focus.
  */
class ExtensionPopupController:
	public CloseDialogContext
{
public:

	ExtensionPopupController(OpWidget& parent);

	OP_STATUS LoadDocument(OpWindowCommander* commander, const OpStringC& url);
	OP_STATUS ReloadDocument(OpGadget* extension, const OpStringC& url);

	BOOL HandlePopupAction(OpInputAction* action);

	void SetContentSize(UINT32 width, UINT32 height);
	
	OP_STATUS AddPageListener(OpPageListener* listener);

private:

	virtual void InitL();

	OpWidget*			m_parent;
	QuickBrowserView*   m_popup_browser_view;
};

#endif // OP_EXTENSIONPOPUPCONTROLLER_H
