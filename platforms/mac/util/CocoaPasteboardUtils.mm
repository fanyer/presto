#include "core/pch.h"

#include "platforms/mac/util/CocoaPasteboardUtils.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/util/systemcapabilities.h"

#include "modules/pi/OpBitmap.h"

#define BOOL NSBOOL
#import <AppKit/NSImage.h>
#undef BOOL

NSString* kOperaDragType = @"com.operasoftware.operainternal";

@implementation CocoaPasteboardUtils

+ (OP_STATUS)PlaceText:(const uni_char *)text pasteboard:(NSPasteboard*)pb
{
	if(text)
	{
		NSArray *types = [NSArray arrayWithObjects: NSStringPboardType, nil];
		NSString *str = [NSString stringWithCharacters:(const unichar*)text length:uni_strlen(text)];
		if(pb && types && str)
		{
			[pb addTypes:types owner:nil];
			[pb setString:str forType:NSStringPboardType];
			return OpStatus::OK;
		}
	}
	
	return OpStatus::ERR;
}

+ (OP_STATUS)PlaceURL:(const uni_char *)url pasteboard:(NSPasteboard*)pb
{
	if(url)
	{
		NSArray *types = [NSArray arrayWithObjects:(NSString*)kUTTypeURL, nil];
		NSString *str = [NSString stringWithCharacters:(const unichar*)url length:uni_strlen(url)];
		if(pb && types && str)
		{
			[pb addTypes:types owner:nil];
			[pb setString:str forType:(NSString*)kUTTypeURL];
			return OpStatus::OK;
		}
	}
	
	return OpStatus::ERR;
}

+ (OP_STATUS)PlaceBitmap:(const OpBitmap*)bitmap pasteboard:(NSPasteboard*)pb
{
	OP_STATUS retval = OpStatus::ERR;

	if(!bitmap || !pb)
		return retval;

	NSArray *types = [NSArray arrayWithObjects: NSTIFFPboardType, nil];
	if(types)
	{
		NSImage *img = (NSImage *)GetNSImageFromOpBitmap(const_cast<OpBitmap*>(bitmap));
		if(img)
		{
			NSData *data = [img TIFFRepresentation];
			if(data)
			{
				[pb addTypes:types owner:nil];
				[pb setData:data forType:NSTIFFPboardType];
				retval = OpStatus::OK;
			}
			[img release];
		}
	}

	return retval;
}

+ (OP_STATUS)PlaceFilename:(const uni_char *)filename pasteboard:(NSPasteboard*)pb
{
	if(filename)
	{
		NSArray *types = [NSArray arrayWithObjects: NSFilenamesPboardType, nil];
		NSString *str = [NSString stringWithCharacters:(const unichar*)filename length:uni_strlen(filename)];
		if(pb && types && str)
		{
			[pb addTypes:types owner:nil];
			id filelist = [pb propertyListForType:NSFilenamesPboardType];
			if(!filelist)
			{
				filelist = [[NSMutableArray alloc] init];
			}
			[filelist addObject:str];

			[pb setPropertyList:filelist forType:NSFilenamesPboardType];
			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

+ (OP_STATUS)PlaceFilenamePromise:(const uni_char *)filename pasteboard:(NSPasteboard*)pb
{
	if(filename)
	{
		NSArray *types = [NSArray arrayWithObjects: NSFilesPromisePboardType, nil];
		NSString *str = [NSString stringWithCharacters:(const unichar*)filename length:uni_strlen(filename)];
		if(pb && types && str)
		{
			[pb addTypes:types owner:nil];
			id filelist = [pb propertyListForType:NSFilesPromisePboardType];
			if(!filelist)
			{
				filelist = [[NSMutableArray alloc] init];
			}
			[filelist addObject:str];

			[pb setPropertyList:filelist forType:NSFilesPromisePboardType];
			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

+ (OP_STATUS)PlaceDummyDataOnPasteboard:(NSPasteboard*)pb
{
	NSArray *types = [NSArray arrayWithObjects: kOperaDragType, nil];
	if(pb && types)
	{
		[pb addTypes:types owner:nil];
		[pb setData:nil forType:kOperaDragType];
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

+ (OP_STATUS)PlaceHTML:(const uni_char *)html_string pasteboard:(NSPasteboard*)pb
{
	if (html_string)
	{
		// For 10.5 use NSHTMLPboardType
		// For 10.6 and up also use NSPasteboardTypeHTML
		NSArray *types = (GetOSVersion() >= 0x1060) ? [NSArray arrayWithObjects: NSHTMLPboardType, NSPasteboardTypeHTML, nil] : [NSArray arrayWithObjects: NSHTMLPboardType, nil];
		NSString *str = [NSString stringWithCharacters:(const unichar*)html_string length:uni_strlen(html_string)];
		if (pb && types && str)
		{
			[pb addTypes:types owner:nil];
			[pb setString:str forType:NSHTMLPboardType];
			if (GetOSVersion() >= 0x1060)
				[pb setString:str forType:NSPasteboardTypeHTML];
			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

@end
