/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/hardcore/component/Messages.h"

#include "platforms/windows/pluginwrapperhooks.h"
#include "platforms/windows_common/pi_impl/WindowsOpPluginTranslator.h"

static HHOOK hook_proc_mouse = NULL;
static HHOOK hook_proc_call_wnd_proc = NULL;

static LRESULT CALLBACK PluginwrapperMouseHook (int nCode, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK PluginwrapperCallWndProcHook (int nCode, WPARAM wParam, LPARAM lParam);

void InstallPluginWrapperHooks()
{
	hook_proc_mouse = SetWindowsHookEx( WH_MOUSE, PluginwrapperMouseHook, 0, GetCurrentThreadId());
	hook_proc_call_wnd_proc = SetWindowsHookEx( WH_CALLWNDPROC, PluginwrapperCallWndProcHook, 0, GetCurrentThreadId());
}

void RemovePluginWrapperHooks()
{
	if (hook_proc_mouse)
		UnhookWindowsHookEx(hook_proc_mouse);

	if (hook_proc_call_wnd_proc)
		UnhookWindowsHookEx(hook_proc_call_wnd_proc);

}

static LRESULT CALLBACK PluginwrapperMouseHook (int nCode, WPARAM wParam, LPARAM lParam)
{
	MOUSEHOOKSTRUCT * pInfo = reinterpret_cast<MOUSEHOOKSTRUCT*>(lParam);
	HWND hwnd = pInfo->hwnd;
	
	//	If nCode is less than zero, the hook procedure must pass the message to 
	//	the CallNextHookEx function without further processing and should return 
	//	the value returned by CallNextHookEx. 

	if (nCode == HC_ACTION && (wParam == WM_NCMOUSEMOVE || (wParam >= WM_MOUSEMOVE && wParam <= WM_LBUTTONUP)))
	{
		WindowsOpPluginTranslator* wrapper;
		if (OpStatus::IsSuccess(WindowsOpPluginTranslator::GetPluginFromHWND(hwnd, TRUE, wrapper)) && wrapper)
			PostMessage(wrapper->GetPluginWindowHWND(), wParam, 0, 0);
	}

	return CallNextHookEx( hook_proc_mouse, nCode, wParam, lParam);
}

static LRESULT CALLBACK PluginwrapperCallWndProcHook (int nCode, WPARAM wParam, LPARAM lParam)
{
	CWPSTRUCT* message = reinterpret_cast<CWPSTRUCT*>(lParam);

	if (nCode == HC_ACTION)
	{
		WindowsOpPluginTranslator* wrapper;
		if (OpStatus::IsSuccess(WindowsOpPluginTranslator::GetPluginFromHWND(message->hwnd, TRUE, wrapper)) && wrapper)
			wrapper->SubClassPluginWindowproc(message->hwnd);
	}

	return CallNextHookEx( hook_proc_mouse, nCode, wParam, lParam);
}
