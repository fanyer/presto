/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Bazyli Zygan bazyl@opera.com
 */

#ifndef GADGET_GEOLOCATION_DIALOG_H
#define GADGET_GEOLOCATION_DIALOG_H

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/locale/locale-enum.h"

class GadgetGeolocationDialog: public Dialog
{
	public:
		GadgetGeolocationDialog(OpInputContext* ctx);

		OP_STATUS Init(DesktopWindow* parent, OpString host_name );

		virtual void OnInit();

		virtual Str::LocaleString	GetOkTextID()		{ return Str::D_GEOLOCATION_PRIVACY_DIALOG_ACCEPT; } // todo: make const in Dialog
		virtual Str::LocaleString	GetCancelTextID()	{ return Str::S_GEOLOCATION_DENY; } // todo: make const in Dialog


		DialogType			GetDialogType() { return TYPE_YES_NO; }
		virtual const char* GetWindowName() { return "Gadget Geolocation Dialog"; }

		virtual UINT32		OnOk();
		virtual void		OnCancel();	

		OpInputContext*		m_callback_ctx;
		OpString			m_host_name;
};

#endif // DOM_GEOLOCATION_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_GEOLOCATION_DIALOG_H
