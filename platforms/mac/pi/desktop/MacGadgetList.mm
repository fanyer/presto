/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "platforms/mac/pi/desktop/MacGadgetList.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "adjunct/widgetruntime/GadgetConfigFile.h"
#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "modules/gadgets/OpGadgetManager.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL

/*
 * Installed gadgets are kept in config plist file as a dictionary keyed with bundle id
 * and stored as an array of bundleId, name, normalized name, bundle path, widget path inside the bundle
 */


BOOL MacGadgetList::HasChangedSince(time_t last_update_time) const
{
	BOOL			   retVal = FALSE;
	OpString		   confFilePath;
	NSString		  *filePath = nil;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSFileManager     *mgr = [NSFileManager defaultManager];
	NSDate			  *fileModDate = nil;
	NSDate			  *askedDate = [NSDate dateWithTimeIntervalSince1970:last_update_time];
	GetWidgetListFilePath(confFilePath);
	filePath = [NSString stringWithCharacters:(const UniChar *)confFilePath.CStr() length:confFilePath.Length()];
	
	
	if ([mgr fileExistsAtPath:filePath])
	{
		fileModDate = [[mgr fileAttributesAtPath:filePath traverseLink:NO] objectForKey:NSFileModificationDate];
		if ([fileModDate compare:askedDate] != NSOrderedAscending)
		{
			retVal = TRUE;
		}
	}
	[pool release];
	return retVal;
}

OP_STATUS MacGadgetList::CreateGadgetContext(void* aName,void *aNormalizedName, 
											 void *aPath, void *aWgtPath, 
											 OpAutoPtr<GadgetInstallerContext>& context) const
{
	NSString *name			 = (NSString *)aName;
	NSString *normName       = (NSString *)aNormalizedName; 
	NSString *path			 = (NSString *)aPath;
	NSString *truncPath      = [path substringToIndex:[path rangeOfString:normName].location];
	NSMutableString *wgtPath = [NSMutableString stringWithString:path];
	OpString sWgtPath;
	uni_char* str = NULL;
	
	[wgtPath appendString:@"/Contents/Resources/"];
	[wgtPath appendString:(NSString*)aWgtPath];
	
	context.reset(OP_NEW(GadgetInstallerContext, ()));
	OP_ASSERT(NULL != context.get());
	if (NULL == context.get())
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	
	// Converting name
	str = new uni_char[[name length]+1];
	[name getCharacters:(unichar*)str range:NSMakeRange(0, [name length])];
	context->m_display_name.Set(str, [name length]);
	
	// Converting normName
	str = new uni_char[[normName length]+1];
	[normName getCharacters:(unichar*)str range:NSMakeRange(0, [normName length])];
	context->m_normalized_name.Set(str, [normName length]);


	// Converting path
	str = new uni_char[[truncPath length]+1];
	[truncPath getCharacters:(unichar*)str range:NSMakeRange(0, [truncPath length])];
	context->m_dest_dir_path.Set(str, [truncPath length]);
	
	// Converting widget path
	str = new uni_char[[wgtPath length]+1];
	[wgtPath getCharacters:(unichar*)str range:NSMakeRange(0, [wgtPath length])];
	sWgtPath.Set(str, [wgtPath length]);
	
	// Time to load config file
	OpAutoPtr<GadgetConfigFile> install_config(GadgetConfigFile::Read(sWgtPath));
	if (NULL == install_config.get())
	{
		return OpStatus::OK;
	}
	
	// Load gadget class
	OpFile gadget_file;
	RETURN_IF_ERROR(gadget_file.Construct(sWgtPath));
	OpString gadget_dir_path, error_message;
	RETURN_IF_ERROR(gadget_file.GetDirectory(gadget_dir_path));
	OpSharedPtr<OpGadgetClass> gadget_class(g_gadget_manager->CreateClassWithPath(gadget_dir_path, URL_GADGET_INSTALL_CONTENT, NULL, error_message));
	if (NULL == gadget_class.get())
	{
		return OpStatus::OK;
	}
	
	context->m_gadget_class = gadget_class;
	context->m_profile_name.Set(install_config->GetProfileName());
	
	return OpStatus::OK;	
}

OP_STATUS MacGadgetList::CreateGadgetContextFromBundle(void *aBundle, void *normalizedName,
							  OpAutoPtr<GadgetInstallerContext>& context) const
{
	NSBundle *bundle         = (NSBundle *)aBundle;
	NSDictionary *infoDict   = [bundle infoDictionary];
	NSString *name			 = [infoDict valueForKey:@"CFBundleDisplayName"];
	NSString *normName       = (NSString *)normalizedName; 
	NSString *path			 = [bundle bundlePath];
	NSMutableString *wgtPath = [infoDict objectForKey:@"WidgetFilename"];
	
	return CreateGadgetContext((void*)name, (void*)normName, (void*)path, (void*)wgtPath, context);
}


OP_STATUS MacGadgetList::GetAll(
		OpAutoVector<GadgetInstallerContext>& gadget_contexts,
		BOOL list_globally_installed_gadgets) const
{
	OP_STATUS retVal = OpStatus::ERR;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	OpString           confFilePath;
	GetWidgetListFilePath(confFilePath);
	NSMutableDictionary      *list = [NSMutableDictionary dictionaryWithContentsOfFile:[NSString stringWithCharacters:(const UniChar *)confFilePath.CStr() length:confFilePath.Length()]];
	NSArray			  *installedGadgets = [list allValues];
	NSDictionary	  *installedGadget = nil;
	BOOL			   changed = NO;
	NSBundle		  *gadgetBundle = nil;
	NSMutableArray    *markedForDeletion = [NSMutableArray array];
	
	for (unsigned int i = 0; i < [installedGadgets count]; i++)
	{
		installedGadget = [installedGadgets objectAtIndex:i];
		if ((gadgetBundle = [NSBundle bundleWithIdentifier:[installedGadget objectForKey:@"BundleId"]]) == nil)
		{
			// It is possible that bundle won't be found, but it is there, so we need to check
			if ([[NSFileManager defaultManager] fileExistsAtPath:[installedGadget objectForKey:@"InstallationPath"]])
			{
				OpAutoPtr<GadgetInstallerContext> context;
				retVal = CreateGadgetContext((void *)[installedGadget objectForKey:@"DisplayName"], 
											 (void *)[installedGadget objectForKey:@"NormalizedName"],
											 (void *)[installedGadget objectForKey:@"InstallationPath"],
											 (void *)[installedGadget objectForKey:@"WidgetPath"],
											 context);
				if (!OpStatus::IsError(retVal) && NULL != context.get())
				{
					retVal = gadget_contexts.Add(context.get());
					context.release();
				}			
				
			}
			else 
			{
				[markedForDeletion addObject:[installedGadget objectForKey:@"BundleId"]];
				changed = YES;				
			}

		}
		else 
		{
			//Well. Widget exists - let's rumble!
			OpAutoPtr<GadgetInstallerContext> context;
			retVal = CreateGadgetContextFromBundle((void *)gadgetBundle, (void *)[installedGadget objectForKey:@"NormalizedName"], context);
			if (!OpStatus::IsError(retVal) && NULL != context.get())
			{
				retVal = gadget_contexts.Add(context.get());
				context.release();
			}			
		}

	}
	
	// It is possible that during listing we found out that one of the gadgets is in a different place
	// or compeletely gone, so we could be forced to update file list
	for (unsigned int i = 0; i < [markedForDeletion count]; i++)
	{
		[list removeObjectForKey:[markedForDeletion objectAtIndex:i]];
	}
	
	if (changed)
	{
		NSData *plistData = [NSPropertyListSerialization dataFromPropertyList:list
																	   format:NSPropertyListXMLFormat_v1_0
															 errorDescription:NULL];
		if(plistData) 
		{
			[plistData writeToFile:[NSString stringWithCharacters:(const UniChar *)confFilePath.CStr() length:confFilePath.Length()] atomically:YES];
		}			
	}
	[pool release];
	return retVal;
}

OP_STATUS MacGadgetList::GetWidgetListFilePath(OpString& widgetlist_file_path) const
{
	if (OpFileUtils::FindFolder(kPreferencesFolderType, widgetlist_file_path))
	{
		widgetlist_file_path.Append("/com.operasoftware.OperaWidgets.plist");
		
		return OpStatus::OK;
	}
	
	return OpStatus::ERR;
}

OP_STATUS PlatformGadgetList::Create(PlatformGadgetList** gadget_list)
{
	OP_ASSERT(NULL != gadget_list);
	if (NULL == gadget_list)
	{
		return OpStatus::ERR;
	}

	*gadget_list = OP_NEW(MacGadgetList, ());
	RETURN_OOM_IF_NULL(*gadget_list);

	return OpStatus::OK;
}

#endif // WIDGET_RUNTIME_SUPPORT
