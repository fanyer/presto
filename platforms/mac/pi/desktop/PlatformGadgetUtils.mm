/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 */
#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"
#include "adjunct/widgetruntime/GadgetConfigFile.h"
#include "platforms/mac/QuickOperaApp/macaufileutils.h"
#include "modules/util/path.h"
#include "adjunct/desktop_util/string/string_convert.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "adjunct/quick/application.h"
#include "adjunct/quick/managers/CommandLineManager.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL

OpString g_widgetPath;

static void TouchWidgetList()
{
	OpString appListPath;
	NSString *confPath = nil;
	NSMutableDictionary *appList = nil;
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	if (OpFileUtils::FindFolder(kPreferencesFolderType, appListPath))
	{
		appListPath.Append("/com.operasoftware.OperaWidgets.plist");
	}

	confPath = [NSString stringWithCharacters:(const UniChar *)appListPath.CStr() length:appListPath.Length()];
	appList = [NSMutableDictionary dictionaryWithContentsOfFile:confPath];

	// We don't need to do anything. Just store it. During a read - it will be updated
	NSData *plistData = [NSPropertyListSerialization dataFromPropertyList:appList
																   format:NSPropertyListXMLFormat_v1_0
														 errorDescription:NULL];
	if(plistData)
	{
		[plistData writeToFile:confPath atomically:YES];
	}
	[pool release];
}

void PlatformGadgetUtils::Normalize(OpString &str)
{

	static const uni_char default_char = '_';

	// If it's empty string we won't do anything.

	if (str.Length() <= 0)
	{
		return;
	}

	// First phase of normalization.

	for (int i = 0; i < str.Length(); ++i)
	{
		if (!OpPathIsValidFileNameChar(str[i])
				&& str[i] != ' ')
		{
			str[i] = default_char;
		}
	}

	// Second phase of normalization.

	int idx1 = 0, idx2 = 0;

	// Skip trailing/leading default_char.

	idx1 = 0;
	while (str[idx1] == default_char) str[idx1++] = ' ';

	idx1 = str.Length() - 1;
	while (str[idx1] == default_char) str[idx1--] = ' ';

	str.Strip();

	// Replace inner default_char strings to one
	// default_char character.

	OpString output;
	output.Reserve(str.Length());

	idx1 = idx2 = 0;

	while (idx1 < str.Length())
	{
		if (str[idx1] != default_char)
		{
			output[idx2++] = str[idx1++];
		}
		else
		{
			while (str[idx1] == default_char && idx1 < str.Length())
			{
				idx1++;
			}

			output[idx2++] = default_char;
		}
	}

	output[idx2] = '\0';
	str.Set(output, idx2);
}



OP_STATUS PlatformGadgetUtils::GetImpliedGadgetFilePath(OpString& path)
{
	OP_STATUS status = OpStatus::ERR;
	//path.Set(g_widgetPath.CStr());
	//return OpStatus::OK;

	NSAutoreleasePool *pool = [NSAutoreleasePool new];

	NSString *widgetpath = [[[NSBundle mainBundle] infoDictionary] valueForKey:@"WidgetFilename"];

	if (widgetpath)
	{
		NSMutableString *filePath = [[NSMutableString alloc] initWithString:[[NSBundle mainBundle] bundlePath]];

		[filePath appendString:@"/Contents/Resources/"];
		[filePath appendString:widgetpath];
		unichar *chars = new unichar[[filePath length]];
		[filePath getCharacters:chars range:NSMakeRange(0, [filePath length])];
		path.Set("");
		path.Append((uni_char *)chars, [filePath length]);
		[filePath release];
		delete chars;
		status = OpStatus::OK;
	}

	[pool release];

	return status;
}


OP_STATUS PlatformGadgetUtils::GetOperaExecPath(OpString16& exec)
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];

	exec.Set([[[NSBundle mainBundle] bundlePath] UTF8String]);

	[pool release];
	return OpStatus::OK;


}


OP_STATUS PlatformGadgetUtils::GetGadgetProfileName(const OpStringC& gadget_path,
							   OpString& profile_name)
{
	OpAutoPtr<GadgetConfigFile> config_file(
											GadgetConfigFile::Read(gadget_path));
	if (NULL == config_file.get())
	{
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(profile_name.Set(config_file->GetProfileName()));

	return OpStatus::OK;
}

OP_STATUS PlatformGadgetUtils::GetAbsolutePath(IN const OpString& file_path,
											  OUT OpString& absolute_path)
{
	using namespace StringConvert;

	const uni_char *buf = file_path.CStr();
	assert(buf != NULL);

	char *cstring = UTF8FromUTF16(buf);
	assert(cstring != NULL);
	if (cstring == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	char resolved[PATH_MAX+1];
	op_memset(resolved, 0, PATH_MAX+1);

	char *tmp = realpath(cstring, resolved);

	assert(tmp != NULL);
	if (tmp == NULL)
	{
		delete cstring;
		return OpStatus::ERR;
	}

	absolute_path.Set(resolved);

	return OpStatus::OK;
}

OP_STATUS PlatformGadgetUtils::InstallGadget(const OpStringC& path)
{
	OP_STATUS retVal = OpStatus::ERR;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString          *installerPath = [NSString stringWithFormat:@"%@/Contents/Resources/WidgetInstaller.app", [[NSBundle mainBundle] bundlePath]];
	NSString		  *widgetPath    = [NSString stringWithCharacters:(const UniChar *)path.CStr() length:path.Length()];

	if ([[NSWorkspace sharedWorkspace] openFile:widgetPath withApplication:installerPath])
	{
		retVal = OpStatus::OK;
	}

	[pool release];
	return retVal;

}

OP_STATUS PlatformGadgetUtils::ExecuteGadget(
        const GadgetInstallerContext& context)
{
	OP_STATUS retVal = OpStatus::ERR;
	OpString target_path;
	OpStatus::Ignore(OpPathDirFileCombine(target_path,
										  context.m_dest_dir_path, context.m_normalized_name));
	target_path.Append(UNI_L(".app"));
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString *appName = [NSString stringWithCharacters:(const UniChar *)target_path.CStr() length:target_path.Length()];

	if ([[NSWorkspace sharedWorkspace] launchApplication:appName])
	{
		retVal = OpStatus::OK;
	}
	else
	{
		// Well.. we've fail to open this widget. What means that we need to update
		// widget list. In this case - just save it anew
		TouchWidgetList();
	}

	[pool release];
    return retVal;
}


OP_STATUS PlatformGadgetUtils::UninstallGadget(
        const GadgetInstallerContext& context)
{
	OP_STATUS retVal = OpStatus::ERR;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSMutableString *appName = [NSMutableString stringWithCharacters:(const UniChar *)context.m_dest_dir_path.CStr() length:context.m_dest_dir_path.Length()];

	[appName appendFormat:@"%@.app", [NSString stringWithCharacters:(const UniChar *)context.m_normalized_name.CStr() length:context.m_normalized_name.Length()]];

	if ([[NSFileManager defaultManager] removeFileAtPath:appName handler:nil])
	{
		TouchWidgetList();
	}
	[pool release];
    return retVal;
}

#endif // WIDGET_RUNTIME_SUPPORT
