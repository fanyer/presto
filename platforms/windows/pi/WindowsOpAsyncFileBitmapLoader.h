/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPASYNCICONLOADER_H
#define WINDOWSOPASYNCICONLOADER_H

#include "adjunct/desktop_pi/loadicons_pi.h"

#ifndef DESKTOP_ASYNC_ICON_LOADER
class WindowsOpAsyncFileBitmapLoader : public OpAsyncFileBitmapLoader, public MessageObject
{
public:
	WindowsOpAsyncFileBitmapLoader();
	virtual ~WindowsOpAsyncFileBitmapLoader();

	virtual OP_STATUS Init(OpAsyncFileBitmapHandlerListener *listener);
	virtual void Start(OpVector<TransferItemContainer>& transferitems);

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	OpAsyncFileBitmapHandlerListener*	m_listener;
	OpAutoVector<OpString>				m_filenames;		// the filenames of the files to get the icon for. Only accessed from the thread.
	HANDLE								m_thread_handle;

	static unsigned __stdcall IconLoadThreadFunc( void* pArguments );
};
#endif /// !DESKTOP_ASYNC_ICON_LOADER

#endif // WINDOWSOPASYNCICONLOADER_H
