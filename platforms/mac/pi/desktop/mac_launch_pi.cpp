// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#include "core/pch.h"

#include "platforms/mac/bundledefines.h"
#include "platforms/mac/pi/desktop/mac_launch_pi.h"
#include "platforms/mac/util/systemcapabilities.h"

#include "adjunct/desktop_util/string/string_convert.h"

////////////////////////////////////////////////////////////

LaunchPI* LaunchPI::Create()
{
	// Can't use OP_NEW as this is used outside of core
	return new MacLaunchPI();
}

////////////////////////////////////////////////////////////


BOOL MacLaunchPI::Launch(const uni_char *exe_name, int argc, const char* const* argv)
{
	char* mbc = StringConvert::UTF8FromUTF16(exe_name);
	OSStatus err;
	FSRef fsref;

	if(FSPathMakeRef((UInt8*)mbc, &fsref, NULL) == noErr)
	{
		LSApplicationParameters launchParams;
		launchParams.version = NULL;
		launchParams.flags = kLSLaunchDefaults | kLSLaunchAsync;
		launchParams.application = &fsref;
		launchParams.asyncLaunchRefCon = 0;
		launchParams.environment = NULL; // should copy env vars
		CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		for (int i=0;i<argc;i++) {
			CFStringRef arg = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)argv[i], strlen(argv[i]), kCFStringEncodingUTF8, false);
			CFArrayAppendValue(array, arg); // kCFTypeArrayCallBacks will cause it to be retained, ref count now 2.
			CFRelease(arg);
		}
		launchParams.argv = array;
		launchParams.initialEvent = NULL;
		err = LSOpenApplication(&launchParams, NULL);
		CFRelease(launchParams.argv);
		if (err == noErr)
		{
			delete [] mbc;
			return TRUE;
		}
	}
	delete [] mbc;

	return FALSE;
}

////////////////////////////////////////////////////////////

BOOL MacLaunchPI::VerifyExecutable(const uni_char *executable, const uni_char *vendor_name)
{
	SInt32 os_ver;
	Gestalt(gestaltSystemVersion, &os_ver);
	if(os_ver < 0x1050)
	{
		// codesign utility is not available before 10.5, skip verification test
		return TRUE;
	}

	char* exe = StringConvert::UTF8FromUTF16(executable);
	if(!exe)
	{
		return FALSE;
	}
	
	BOOL updater = strstr(exe, "Update.app") ? TRUE : FALSE;
	
	OpBasicString8 cmd_line(""), cmd_line_next("");
	if(vendor_name)
	{
		// See DSK-336700. We need a way to verify the digital file signature using any given vendor name
		// or its Mac counterpart in this case.
		char* vendor_name_narrow = StringConvert::UTF8FromUTF16(vendor_name);
		BuildCodesignCmd(cmd_line, vendor_name_narrow, exe);
		delete [] vendor_name_narrow;
	}
	else
	{
		if(updater)
		{
			BuildCodesignCmd(cmd_line, OPERA_UPDATE_BUNDLE_ID_STRING, exe);
		}
		else
		{
			BuildCodesignCmd(cmd_line, OPERA_BUNDLE_ID_STRING, exe);
			BuildCodesignCmd(cmd_line_next, OPERA_NEXT_BUNDLE_ID_STRING, exe);
		}
	}
	delete [] exe;

	int retval = system(cmd_line.CStr());

	// If the text for Opera.app failed we should try Opera Next
	if (!updater && retval)
		retval = system(cmd_line_next.CStr());

	if(retval == 0)				
		return TRUE;
	else
		return FALSE;		
}

////////////////////////////////////////////////////////////

void MacLaunchPI::BuildCodesignCmd(OpBasicString8 &cmd_line, const char *bundle_id, char *exe)
{
	cmd_line.Append("/usr/bin/codesign ");
	cmd_line.Append("-v -R \"=identifier=");
	cmd_line.Append(bundle_id);
	cmd_line.Append("\" \"");
	cmd_line.Append(exe);
	cmd_line.Append("\"");
}

////////////////////////////////////////////////////////////
