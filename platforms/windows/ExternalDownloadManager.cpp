/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "ExternalDownloadManager.h"

#include "platforms/windows/com/flashget.h"
#include "platforms/windows/com/qqdownload.h"
#include "platforms/windows/com/thunder.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/utils/win_icon.h"
#include "platforms/windows/windows_ui/winshell.h"

// thunder
const IID IID_IAgent = {0x1622F56A,0x0C55,0x464C,0xB4,0x72,0x37,0x78,0x45,0xDE,0xF2,0x1D};
const CLSID CLSID_Agent = {0x485463B7,0x8FB2,0x4B3B,0xB2,0x9B,0x8B,0x91,0x9B,0x0E,0xAC,0xCE};

// qq
const IID IID_IQQRightClick = {0xDCE3B5F0,0x6682,0x4C85,0xAE,0x1F,0x27,0x2B,0x07,0x62,0xE7,0xFD};
const CLSID CLSID_QQRightClick = {0x4836C333,0x208E,0x4BCE,0xB3,0x0B,0x00,0xB9,0x54,0x5B,0x0F,0x6E};

// flashget
const CLSID CLSID_IFlashGetNetscape = {0x116BA71C,0x8187,0x4F15,0x9A,0x1F,0xC9,0xD6,0x28,0x91,0x55,0xD1};
const IID IID_IIFlashGetNetscape = {0x6DD9E779,0x2707,0x4BF0,0x82,0x69,0xE4,0xC6,0xBD,0x8B,0x39,0xB7};


ExternalDownloadManager* ThunderDownloadManager::Construct()
{
	// Check if the client exist
	IAgent* thunder;
	HRESULT hr = CoCreateInstance(
				CLSID_Agent,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IAgent,
				(void**)&thunder
				);
	
	if (FAILED(hr))
		return NULL;
	else
		thunder->Release();

	ThunderDownloadManager* manager = OP_NEW(ThunderDownloadManager,());
	if (!manager)
		return NULL;

	OpString dummy;
	if (GetProtocolInfo(UNI_L("thunder"), manager->name, dummy, manager->application_icon))
	{
		return manager;
	}

	OP_DELETE(manager);
	return NULL;
}

OP_STATUS ThunderDownloadManager::Run(const uni_char* url)
{
	IAgent* thunder;
	HRESULT hr = CoCreateInstance(
				CLSID_Agent,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IAgent,
				(void**)&thunder
				);
	if (FAILED(hr) || !thunder)
		return OpStatus::ERR;

	BSTR str = SysAllocString(url);
	if (!str)
		return OpStatus::ERR_NO_MEMORY;

	INT32 ret = 0;
	if (FAILED(thunder->AddTask(str))
		|| FAILED(thunder->CommitTasks(&ret)))
	{
		thunder->Release();
		SysFreeString(str);
		return OpStatus::ERR;
	}

	thunder->Release();
	SysFreeString(str);
	return OpStatus::OK;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

ExternalDownloadManager* QQDownloadManager::Construct()
{
	// Check if the client exist
	IQQRightClick* qq;
	HRESULT hr = CoCreateInstance(
				CLSID_QQRightClick,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IQQRightClick,
				(void**)&qq
				);
	
	if (FAILED(hr))
		return NULL;
	else
		qq->Release();

	QQDownloadManager* manager = OP_NEW(QQDownloadManager,());
	if (!manager)
		return NULL;

	OpString dummy;
	if (GetProtocolInfo(UNI_L("qqdl"), manager->name, dummy, manager->application_icon))
	{
		return manager;
	}

	OP_DELETE(manager);
	return NULL;
}

OP_STATUS QQDownloadManager::Run(const uni_char* url)
{
	IQQRightClick* qq;
	HRESULT hr = CoCreateInstance(
				CLSID_QQRightClick,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IQQRightClick,
				(void**)&qq
				);

	if (FAILED(hr) || !qq)
		return OpStatus::ERR;

	BSTR str = SysAllocString(url);
	if (!str)
		return OpStatus::ERR_NO_MEMORY;

	if (FAILED(qq->SendUrl(str, NULL, NULL, NULL)))
	{
		qq->Release();
		SysFreeString(str);
		return OpStatus::ERR;
	}

	qq->Release();
	SysFreeString(str);
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
ExternalDownloadManager* FlashgetDownloadManager::Construct()
{
	// Check if the client exist
	IIFlashGetNetscape* flashget;
	HRESULT hr = CoCreateInstance(
				CLSID_IFlashGetNetscape,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IIFlashGetNetscape,
				(void**)&flashget
				);
	
	if (FAILED(hr))
		return NULL;
	else
		flashget->Release();

	FlashgetDownloadManager* manager = OP_NEW(FlashgetDownloadManager,());
	if (!manager)
		return NULL;

	OpString dummy;
	if (GetProtocolInfo(UNI_L("flashget"), manager->name, dummy, manager->application_icon))
	{
		return manager;
	}

	OP_DELETE(manager);
	return NULL;
}

OP_STATUS FlashgetDownloadManager::Run(const uni_char* url)
{
	// Check if the client exist
	IIFlashGetNetscape* flashget;
	HRESULT hr = CoCreateInstance(
				CLSID_IFlashGetNetscape,
				NULL,
				CLSCTX_INPROC_SERVER,
				IID_IIFlashGetNetscape,
				(void**)&flashget
				);
	
	if (FAILED(hr))
		return OpStatus::ERR;

	BSTR str = SysAllocString(url);
	if (!str)
		return OpStatus::ERR_NO_MEMORY;

	if (FAILED(flashget->AddUrl(str, NULL, NULL)))
	{
		flashget->Release();
		SysFreeString(str);
		return OpStatus::ERR;
	}

	flashget->Release();
	SysFreeString(str);
	return OpStatus::OK;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

BOOL GetProtocolInfo(const uni_char* protocol, OpString& name, OpString& application, Image& icon)
{
	uni_char command[512];
	
	if (!GetShellProtocolHandler(protocol, NULL, 0, command, 512))
		return FALSE;

	OpString dummy;
	if (OpStatus::IsError(WindowsOpSystemInfo::ProcessAppParameters(command, NULL, application, dummy)))
		return FALSE;

	if (!g_op_system_info->GetIsExecutable(&application))
		return FALSE;

	GetFileDescription(application.CStr(), NULL, &name);

	OpBitmap* bitmap = NULL;
	IconUtils::CreateIconBitmap(&bitmap, application.CStr(), 16);
	if (bitmap)
		icon = imgManager->GetImage(bitmap);

	return TRUE;
}

