/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Bazyli Zygan bazyl@opera.com
 */

#ifndef GADGET_REMOTE_DEBUG_DIALOG_H
#define GADGET_REMOTE_DEBUG_DIALOG_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/widgetruntime/GadgetRemoteDebugHandler.h"


class GadgetRemoteDebugDialog
	: public Dialog,
	  public GadgetRemoteDebugHandler::Listener
{
public:
	GadgetRemoteDebugDialog();
	virtual ~GadgetRemoteDebugDialog();
	
	/**
	 * 
	 */
	DialogType GetDialogType() { return TYPE_CLOSE; }
	virtual const char* GetWindowName() { return "Gadget Remote Debug Settings Dialog"; }
		
	/**
	 * Dialog initialization.
	 * 
	 * @return Error status.
	 */
	OP_STATUS Init(GadgetRemoteDebugHandler& debug_handler);
		
	
	//
	// GadgetRemoteDebugHandler::Listener
	//
	virtual void OnConnectionSuccess();
	virtual void OnConnectionFailure();

	//
	// OpWidgetListener
	//
	virtual void OnClick(OpWidget* widget, UINT32 id);
	
private:
	void SetEditEnabled(BOOL enable);
	void SetDefaults();
	void ParseIpAddress(const OpStringC& ip_address, int port);
	int  NumberFromString(OpString& str);
	
	void Connect();
	void Disconnect();

	GadgetRemoteDebugHandler* m_debug_handler;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_REMOTE_DEBUG_DIALOG_H
