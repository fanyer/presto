/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if defined(USE_OPDLL)

#ifdef MACGOGI
#include "platforms/macgogi/pi/MacOpDLL.h"
#else
#include "platforms/mac/pi/MacOpDLL.h"
#endif

#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"
#include "platforms/mac/util/systemcapabilities.h"
#include <dlfcn.h>

OP_STATUS OpDLL::Create(OpDLL** newObj)
{
	*newObj = new MacOpDLL();
	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

MacOpDLL::MacOpDLL()
  : loaded(FALSE),
	library(NULL),
	dll(NULL)
{
}

MacOpDLL::~MacOpDLL()
{
	if(loaded)
		Unload();
}

OP_STATUS MacOpDLL::Load(const uni_char* dll_name)
{
	FSRef 		frameworksFolderRef;
	FSRef		dllRef;
	CFURLRef 	baseURL;
	CFURLRef 	bundleURL;
	CFBundleRef mainBundle;
	CFURLRef 	privateFrameworksURL;
	CFURLRef 	privateBundleURL;
	OSStatus 	err;

	if(loaded)
		Unload();
	library = nil;
	OpString expanded_dll_name;
	expanded_dll_name.Reserve(2048); // FIXME: Proper macro

	OP_STATUS status = g_op_system_info->ExpandSystemVariablesInString(dll_name, &expanded_dll_name);

	m_utf8_path.SetUTF8FromUTF16(OpStatus::IsSuccess(status)?expanded_dll_name.CStr():dll_name);
	dll = dlopen(m_utf8_path.CStr(), RTLD_LAZY);
	if (dll) {
		loaded = true;
		return OpStatus::OK;
	}

	if(OpFileUtils::ConvertUniPathToFSRef(OpStatus::IsSuccess(status)?expanded_dll_name.CStr():dll_name, dllRef))
	{
		bundleURL = CFURLCreateFromFSRef(kCFAllocatorDefault, &dllRef);
		if(bundleURL)
		{
			library = CFBundleCreate(kCFAllocatorDefault, bundleURL);
			CFRelease(bundleURL);
			if(library)
			{
				if (CFBundleLoadExecutable(library))
				{
					loaded = TRUE;
					return OpStatus::OK;
				}
				else
				{
					CFRelease(library);
					library = NULL;
				}
			}
		}
	}

	err = FSFindFolder(kOnAppropriateDisk, kFrameworksFolderType, true, &frameworksFolderRef);
	if(err == noErr)
	{
		baseURL = CFURLCreateFromFSRef(kCFAllocatorDefault, &frameworksFolderRef);
		if (baseURL)
		{
			CFStringRef framework = CFStringCreateWithCharacters(NULL, (UniChar *) dll_name, uni_strlen(dll_name));
			if(framework)
			{
				bundleURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, baseURL, framework, false);
				if (bundleURL)
				{
					library = CFBundleCreate(kCFAllocatorDefault, bundleURL);
					if (library)
					{
						if (CFBundleLoadExecutable(library))
						{
							loaded = TRUE;
						}
						else
						{
							CFRelease(library);
							library = nil;
						}
					}
					else
					{
						mainBundle = CFBundleGetMainBundle();
						if(mainBundle)
						{
							privateFrameworksURL = CFBundleCopyPrivateFrameworksURL(mainBundle);
							if(privateFrameworksURL)
							{
								privateBundleURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, privateFrameworksURL, framework, false);

								if(privateBundleURL)
								{
									library = CFBundleCreate(kCFAllocatorDefault, privateBundleURL);
									if(library)
									{
										if (CFBundleLoadExecutable(library))
										{
											loaded = TRUE;
										}
										else
										{
											CFRelease(library);
											library = nil;
										}
									}
									CFRelease(privateBundleURL);
								}
								CFRelease(privateFrameworksURL);
							}
							// DONT release on Get..., only release on Create...
	//						CFRelease(mainBundle);
						}
					}
					CFRelease(bundleURL);
				}
				CFRelease(framework);
			}
			CFRelease(baseURL);
		}
	}

	if(!loaded)
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS MacOpDLL::Unload()
{
	if (loaded && library)
	{
		if (!m_utf8_path.CStr() ||
				!op_stristr(m_utf8_path.CStr(), "Silverlight") &&	// For Silverlight 5.1
				(GetOSVersion() >= 0x1060 || !op_stristr(m_utf8_path.CStr(), "Flash")) &&		// For Flash 10.2
				!op_stristr(m_utf8_path.CStr(), "RealPlayer") &&		// For RealPlayer
				!op_stristr(m_utf8_path.CStr(), "QuakeLivePlugin")		// For Quake Live 0.1
				)
		{
			CFRelease(library);
		}
		library = NULL;
	}
	if (dll)
	{
		dlclose(dll);
		dll = NULL;
	}
	loaded = FALSE;
	return OpStatus::OK;
}

void *MacOpDLL::GetSymbolAddress(const char *symbol_name) const
{
	if (dll)
		return dlsym(dll, symbol_name);

	CFStringRef symbol = CFStringCreateWithCString(kCFAllocatorDefault, symbol_name, kCFStringEncodingUTF8);
	void *result = CFBundleGetFunctionPointerForName(library, symbol);
	CFRelease(symbol);

	return result;
}

#endif
