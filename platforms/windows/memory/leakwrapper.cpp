/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

// ensure that this class is constructed before other user generated global classes, otherwise global user classes
// will appear as false positives - pettern 18.01.2009
#pragma warning(disable : 4073)
#pragma init_seg (lib)

#if defined(_DEBUG)
static class DumpMemoryWrapper
{
public:
	DumpMemoryWrapper()
	{
	}

	~DumpMemoryWrapper()
	{
#if defined(_DEBUG)
#ifdef ENABLE_MEMORY_DEBUGGING
		OpMemDebug_ReportLeaks();
#endif // ENBALE_MEMORY_DEBUGGING
#if defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC)
		extern void free_WindowsCodeLocationManager();

		free_WindowsCodeLocationManager();
#endif // MEMTOOLS_ENABLE_CODELOC && MEMTOOLS_NO_DEFAULT_CODELOC
#endif // _DEBUG
	}
} dump_memory_wrapper;
#endif 

#ifdef _DEBUG
class Win32ObjectLeakCheck
{
public:
	Win32ObjectLeakCheck(const OpString& identifier);
	Win32ObjectLeakCheck(const uni_char* identifier);
	Win32ObjectLeakCheck(const OpString& className, void* instance);
	~Win32ObjectLeakCheck();

	static unsigned int GetCurrentGDIObjectsInUse();
	static unsigned int GetCurrentUserObjectsInUse();

private:
	void Init(const OpString& identifier);
	OpString m_identifier;
	unsigned int m_gdi_startCount;
	unsigned int m_gdi_endCount;
	unsigned int m_user_startCount;
	unsigned int m_user_endCount;

} leakcheck(UNI_L("Opera"));

Win32ObjectLeakCheck::Win32ObjectLeakCheck(const OpString& identifier)
{
	Init(identifier);
}

Win32ObjectLeakCheck::Win32ObjectLeakCheck(const uni_char* identifier)
{
	OpString tmp;

	tmp.Set(identifier);

	Init(tmp);
}

Win32ObjectLeakCheck::Win32ObjectLeakCheck(const OpString& classname, void* instance)
{
	OpString tmp;

	tmp.AppendFormat(UNI_L("%s (0x%08x)"), classname.CStr(), instance);

	Init(tmp);
}

void Win32ObjectLeakCheck::Init(const OpString& identifier)
{
	m_identifier.Set(identifier.CStr());
	m_gdi_startCount = GetCurrentGDIObjectsInUse();
	m_user_startCount = GetCurrentGDIObjectsInUse();
	m_gdi_endCount = m_user_endCount = 0;
}

Win32ObjectLeakCheck::~Win32ObjectLeakCheck()
{
	uni_char tmpbuf[256];

	m_user_endCount = GetCurrentUserObjectsInUse();
	int leaked = m_user_endCount - m_user_startCount;
	if (leaked != 0)
	{
		// if this leaks, then read http://msdn.microsoft.com/en-us/library/ms725486(VS.85).aspx
		wsprintf(tmpbuf, UNI_L("*** %.200s: leaked %d USER object%s! ***\n"), m_identifier.CStr(), leaked, leaked == 1 ? UNI_L("") : UNI_L("s"));
		OutputDebugString(tmpbuf);
	}
	m_gdi_endCount = GetCurrentGDIObjectsInUse();
	leaked = m_gdi_endCount - m_gdi_startCount;
	if (leaked != 0)
	{
		wsprintf(tmpbuf, UNI_L("*** %.200s: leaked %d GDI object%s! ***\n"), m_identifier.CStr(), leaked, leaked == 1 ? UNI_L("") : UNI_L("s"));
		OutputDebugString(tmpbuf);
	}
}

unsigned int Win32ObjectLeakCheck::GetCurrentGDIObjectsInUse()
{
	return ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
}

unsigned int Win32ObjectLeakCheck::GetCurrentUserObjectsInUse()
{
	return ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
}
#endif // _DEBUG

