/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#ifndef SIGNATUREEDITOR_H
#define SIGNATUREEDITOR_H

#include "adjunct/quick/dialogs/URLInputDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "adjunct/m2_ui/widgets/RichTextEditorListener.h"

class SignatureEditor : public Dialog, public RichTextEditorListener, public URLInputDialog::URLInputListener
{
public:

		SignatureEditor()
		: m_account_id(0)
		, m_rich_text_editor(NULL)
		{
		}

		// override Dialog's init
		void						Init(DesktopWindow *parent_window, UINT32 account_id);
		
		virtual						~SignatureEditor() {};
		
		// Dialog
		virtual void				OnInit();
		virtual DialogType			GetDialogType()	{ return TYPE_OK_CANCEL; }
		virtual Str::LocaleString	GetOkTextID();
		virtual UINT32				OnOk();
		
		// OpTypedObject
		virtual Type				GetType()				{ return DIALOG_TYPE_SIGNATURE_EDITOR; }
		virtual const char*			GetWindowName()			{ return "Signature Editor"; }
		const char*					GetHelpAnchor()			{return "mail.html#compose";}


		// OpInputContext
		virtual BOOL				OnInputAction(OpInputAction* action);

		// RichTextEditorListener
		virtual void				OnTextChanged() {}
		virtual void				OnInsertImage();

		// URLInputListener
		virtual void				OnURLInputOkCallback (OpString& url_result);

private: 
		UINT32						m_account_id;
		RichTextEditor*				m_rich_text_editor;
};
#endif //SIGNATUREEDITOR_H
