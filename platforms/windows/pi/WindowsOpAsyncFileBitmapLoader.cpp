/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/pi/WindowsOpAsyncFileBitmapLoader.h"
#include "platforms/windows/pi/WindowsOpThreadTools.h"
#include "adjunct/desktop_util/handlers/TransferItemContainer.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "modules/pi/OpBitmap.h"
#include <process.h>

#ifndef DESKTOP_ASYNC_ICON_LOADER
OP_STATUS OpAsyncFileBitmapLoader::Create(OpAsyncFileBitmapLoader** new_asyncfilebitmaploader)
{
	OP_ASSERT(new_asyncfilebitmaploader != NULL);
	*new_asyncfilebitmaploader = OP_NEW(WindowsOpAsyncFileBitmapLoader, ());
	if(*new_asyncfilebitmaploader == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

WindowsOpAsyncFileBitmapLoader::WindowsOpAsyncFileBitmapLoader() :
	m_listener(NULL),
	m_thread_handle(NULL)
{
}

WindowsOpAsyncFileBitmapLoader::~WindowsOpAsyncFileBitmapLoader()
{
	if(m_thread_handle)
	{
		WaitForSingleObject(m_thread_handle, INFINITE);
		CloseHandle(m_thread_handle);
	}
	g_main_message_handler->UnsetCallBacks(this);
}

OP_STATUS WindowsOpAsyncFileBitmapLoader::Init(OpAsyncFileBitmapHandlerListener *listener)
{
	m_listener = listener;
	
	g_main_message_handler->SetCallBack(this, MSG_WIN_ICON_LOADED, 0);
	g_main_message_handler->SetCallBack(this, MSG_WIN_ICON_LOADING_FINISHED, 0);

	return OpStatus::OK;
}

void WindowsOpAsyncFileBitmapLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_WIN_ICON_LOADED || msg == MSG_WIN_ICON_LOADING_FINISHED);

	if(msg == MSG_WIN_ICON_LOADED)
	{
		bool bitmap_set = false;
		uni_char *filename = reinterpret_cast<uni_char *>(par2);
		OpBitmap *iconbitmap = reinterpret_cast<OpBitmap *>(par1);
		if(filename)
		{
			// find the TransferItemContainer based on the filename
			TransferItemContainer* item = g_desktop_transfer_manager->FindTransferItemContainerFromFilename(filename);
			if(item)
			{
				if (iconbitmap)
				{
					item->SetIconBitmap(imgManager->GetImage(iconbitmap));
					m_listener->OnBitmapLoaded(item);
					bitmap_set = true;
				}

				item->SetHasTriedToLoadIcon();
			}
		}
		if(!bitmap_set)
			OP_DELETE(iconbitmap);

		op_free(filename);
	}
	else if(msg == MSG_WIN_ICON_LOADING_FINISHED)
	{
		m_listener->OnBitmapLoadingDone();
	}
}

void WindowsOpAsyncFileBitmapLoader::Start(OpVector<TransferItemContainer>& transferitems)
{
	OP_ASSERT(transferitems.GetCount());

	if(!m_thread_handle && transferitems.GetCount())
	{
		for(UINT32 n = 0; n < transferitems.GetCount(); n++)
		{
			TransferItemContainer *item = transferitems.Get(n);
			if(item->NeedToLoadIcon() && item->GetAssociatedItem())
			{
				OpAutoPtr<OpString> filename(OP_NEW(OpString, ()));
				if(filename.get())
				{
					if(OpStatus::IsSuccess(filename->Set(*item->GetAssociatedItem()->GetStorageFilename())) &&
						OpStatus::IsSuccess(m_filenames.Add(filename.get())))
						filename.release();
				}
			}
		}
		if(m_filenames.GetCount())
			m_thread_handle = (HANDLE)_beginthreadex( NULL, 0, &IconLoadThreadFunc, (void *)this, 0, NULL);
	}
}

/* static */
unsigned __stdcall WindowsOpAsyncFileBitmapLoader::IconLoadThreadFunc( void* pArguments )
{
	// get the class we will operate on
	WindowsOpAsyncFileBitmapLoader* _this = reinterpret_cast<WindowsOpAsyncFileBitmapLoader *>(pArguments);
	// get a reference to all the data will we deal with
	OpAutoVector<OpString> *filenames = &_this->m_filenames;

	for(UINT32 n = 0; n < filenames->GetCount(); n++)
	{
		OpBitmap* iconbitmap = NULL;
		uni_char *filename = uni_strdup(filenames->Get(n)->CStr());

		if(filename && OpStatus::IsSuccess(DesktopPiUtil::CreateIconBitmapForFile(&iconbitmap, filename)))
		{
			g_thread_tools->PostMessageToMainThread(MSG_WIN_ICON_LOADED, (MH_PARAM_1)iconbitmap, (MH_PARAM_2)filename);
		}
	}
	filenames->DeleteAll();
	_this->m_thread_handle = NULL;

	g_thread_tools->PostMessageToMainThread(MSG_WIN_ICON_LOADING_FINISHED, 0, 0);
	return 0;
}

#endif // DESKTOP_ASYNC_ICON_LOADER
