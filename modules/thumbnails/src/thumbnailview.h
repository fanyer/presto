/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef THUMBNAILMANAGER_THUMBNAILVIEW_H_
#define THUMBNAILMANAGER_THUMBNAILVIEW_H_

#ifdef CORE_THUMBNAIL_SUPPORT

#include "modules/widgets/OpWidget.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/url/url_man.h"

class OpWindowCommander;
class OpWindow;

class OpThumbnailView :
	public OpWidget,
	public NullDocumentListener
{
public:
	OpThumbnailView();
	virtual ~OpThumbnailView();

	static OP_STATUS Construct(OpThumbnailView** obj);

	OP_STATUS Init();

	Window* GetWindow() const;

	// From OpWidget
	virtual void OnAdded();
	virtual void OnDeleted();

	// From OpDocumentListener
	virtual void OnJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, OpDocumentListener::JSDialogCallback *callback) { callback->AbortScript();}
	virtual void OnJSAlert(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback) { callback->AbortScript(); }
	virtual void OnConfirm(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback) { callback->OnDialogReply(DialogCallback::REPLY_CANCEL); }
	virtual void OnAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, DialogCallback* callback) { callback->OnDialogReply(DialogCallback::REPLY_CANCEL); }
	virtual void OnAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, DialogCallback* callback)  { callback->OnDialogReply(DialogCallback::REPLY_CANCEL); }
	virtual void OnGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height) { m_op_window->GetInnerSize(width, height); }

private:
	OpWindowCommander*	m_window_commander;
	OpWindow*			m_op_window;
};

#endif // CORE_THUMBNAIL_SUPPORT
#endif // THUMBNAILMANAGER_THUMBNAILVIEW_H_
