/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#ifndef URLINPUTDIALOG_H
#define URLINPUTDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class URLInputDialog : public Dialog
{
public:

	/* The URLInputListener is used by the URLInputDialog to carry the result to
	 * the requester
	 */
	class URLInputListener {
	public:
		virtual ~URLInputListener(){}

		virtual void OnURLInputOkCallback (OpString& url_result) = 0;
		virtual void OnURLInputCancelCallback () {}
	};


	// Dialog
	virtual void				OnInit();
	virtual DialogType			GetDialogType()	{ return TYPE_OK_CANCEL; }
	
	virtual UINT32				OnOk();
	virtual void				OnCancel();
		
	// OpTypedObject
	virtual Type				GetType()				{ return DIALOG_TYPE_URL_INPUT; }
	virtual const char*			GetWindowName()			{ return "URL Input Dialog"; }


	void						SetTitleAndMessage( const OpStringC& title, const OpStringC& message);
	virtual	void				SetURLInputListener(URLInputListener* listener) { m_url_input_listener = listener; }

private: 

	URLInputListener*			m_url_input_listener;

};
#endif //URLINPUTDIALOG_H
