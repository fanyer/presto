/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/util/htmltotext.h"

#include "adjunct/quick/Application.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/layout/cascade.h"


/***********************************************************************************
 ** Convert the HTML part of the currently visible mail message to text
 **
 ** NB TODO This function was moved here from m2glue.cpp to isolate the madness.
 **    This function is nasty and shouldn't be here; core should have a function
 **    available to convert HTML to text. See bug #328157.
 **
 ** HTMLToText::GetCurrentHTMLMailAsText
 ** @param message_id ID of Message to convert, only used to check if this is
 **        current message
 ** @param plain_text Plain text output
 ***********************************************************************************/
OP_STATUS HTMLToText::GetCurrentHTMLMailAsText(message_gid_t message_id, OpString16& plain_text)
{
	// Currently, the only way to get text from html-mail, is to have a dummy document
	// that displays the mail, and "save" the document as plain_text.
	plain_text.Empty();

	MailDesktopWindow* mail_window = g_application->GetActiveMailDesktopWindow();

	if (!mail_window													||
		(message_gid_t)mail_window->GetCurrentMessageID() != message_id ||
		!mail_window->GetMailView()										||
		!mail_window->GetMailView()->GetWindow()						)
	{
		return OpStatus::ERR;
	}

	Window* window = mail_window->GetMailView()->GetWindow();
	OpAutoPtr<UnicodeStringOutputStream> string_stream (OP_NEW(UnicodeStringOutputStream, ()));

	if (!string_stream.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(string_stream->Construct(&plain_text));

	DocumentManager* doc_manager = window->DocManager();
	if (doc_manager)
	{
		FramesDocument* document = doc_manager->GetCurrentDoc();
		if (document)
		{
			HTML_Element* root = document->GetDocRoot();
			if (root)
			{
				document->Reflow(FALSE);

				HTML_Element* mail_object = root->GetElmById(UNI_L("omf_body_part_1"));
				if (mail_object)
				{
					FramesDocElm* mail_frm = FramesDocElm::GetFrmDocElmByHTML(mail_object);
					FramesDocument* mail_doc = mail_frm ? mail_frm->GetCurrentDoc() : NULL;
					if (mail_doc && mail_doc->Type() == DOC_FRAMES)
					{
						LogicalDocument* mail_log_doc = ((FramesDocument*)mail_doc)->GetLogicalDocument();
						if (mail_log_doc && mail_log_doc->GetRoot())
						{
							int line_length = -1;
							BOOL trailing_ws = FALSE;
							BOOL prevent_newline = FALSE;
							BOOL pending_newline = FALSE;

							HLDocProfile* hld_profile = mail_log_doc->GetHLDocProfile();
							if (hld_profile)
							{
								Head props_list;

								LayoutProperties* cascade = new LayoutProperties;
								if (!cascade)
									return OpStatus::ERR_NO_MEMORY;

								if (hld_profile->GetVisualDevice())
									hld_profile->GetVisualDevice()->Reset();

								// Set ua default values
								cascade->GetProps()->Reset();
								cascade->Into(&props_list);

								OpStatus::Ignore(mail_log_doc->GetRoot()->WriteAsText(
												string_stream.get(),
												hld_profile,
												cascade,
												80,
												line_length,
												trailing_ws,
												prevent_newline,
												pending_newline));

								props_list.Clear();
							}
						}
					}
				}
			}
		}
	}

	return OpStatus::OK;
}


#endif // M2_SUPPORT
