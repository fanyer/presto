/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NO_CARBON

#define BOOL NSBOOL
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Foundation/NSData.h>
#import <AppKit/NSPasteboard.h>
#undef BOOL

#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "platforms/mac/pi/CocoaOpClipboard.h"
#include "platforms/mac/CocoaVegaDefines.h"
#include "modules/pi/OpBitmap.h"

CocoaOpClipboard* CocoaOpClipboard::s_cocoa_op_clipboard = NULL;

OP_STATUS OpClipboard::Create(OpClipboard** new_opclipboard)
{
	*new_opclipboard = new CocoaOpClipboard;
	if (*new_opclipboard == NULL)
		return OpStatus::ERR_NO_MEMORY;

	CocoaOpClipboard::s_cocoa_op_clipboard = (CocoaOpClipboard*)*new_opclipboard;

	return OpStatus::OK;
}

CocoaOpClipboard::CocoaOpClipboard()
	: m_service_pboard(nil)
	, m_service_types(nil)
{
}

CocoaOpClipboard::~CocoaOpClipboard()
{
	s_cocoa_op_clipboard = NULL;
}

BOOL CocoaOpClipboard::HasText()
{
	NSPasteboard *pb = m_service_pboard ? m_service_pboard : [NSPasteboard generalPasteboard];
	NSArray *types = [NSArray arrayWithObjects: NSStringPboardType, nil];
	if (pb && types)
	{
		NSString *bestType = [pb availableTypeFromArray:types];
		return (bestType != nil);
	}
	return FALSE;
}

OP_STATUS CocoaOpClipboard::PlaceText(const uni_char* text, UINT32 token)
{
	if (text)
	{
		NSPasteboard *pb = m_service_pboard ? m_service_pboard : [NSPasteboard generalPasteboard];
		if (m_service_types && ![m_service_types containsObject:NSStringPboardType])
			return OpStatus::OK;
		NSArray *types = [NSArray arrayWithObjects: NSStringPboardType, nil];
		NSString *str = [NSString stringWithCharacters:(const unichar*)text length:uni_strlen(text)];
		if (pb && types && str)
		{
			[pb declareTypes:types owner:nil];
			[pb setString:str forType:NSStringPboardType];
			m_owner_token = token;
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

OP_STATUS CocoaOpClipboard::GetText(OpString &text)
{
	NSPasteboard *pb = m_service_pboard ? m_service_pboard : [NSPasteboard generalPasteboard];
	NSArray *types = [NSArray arrayWithObjects: NSStringPboardType, nil];
	if (pb && types)
	{
		NSString *bestType = [pb availableTypeFromArray:types];
		if (bestType)
		{
			NSString *str = [pb stringForType:NSStringPboardType];
			if (str && text.Reserve([str length] + 1))
			{
				[str getCharacters:(unichar*)text.CStr() range:NSMakeRange(0, [str length])];
				text.CStr()[[str length]] = '\0';
				return OpStatus::OK;
			}
		}
	}
	return OpStatus::ERR;
}

static void DeleteBuffer(void *info, const void *data, size_t size)
{
	free(info);
}

OP_STATUS CocoaOpClipboard::PlaceBitmap(const OpBitmap* bitmap, UINT32 token)
{
	OP_STATUS retval = OpStatus::ERR;
	
	if (!bitmap)
		return retval;

	NSPasteboard *pb = m_service_pboard ? m_service_pboard : [NSPasteboard generalPasteboard];
	if (m_service_types && ![m_service_types containsObject:NSTIFFPboardType])
		return OpStatus::OK;
	NSArray *types = [NSArray arrayWithObjects: NSTIFFPboardType, nil];

	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	if (colorSpace)
	{
		int bpl = bitmap->GetBpp() < 32 ? bitmap->Width()*4 : bitmap->GetBytesPerLine();
		size_t data_size = bpl*bitmap->Height();
		void* bitmap_data = malloc(data_size);
		if (bitmap_data)
		{
			const void* source_data = const_cast<OpBitmap*>(bitmap)->GetPointer(OpBitmap::ACCESS_READONLY);
			if (bitmap->GetBpp() < 32 || !source_data)
			{
				for (unsigned int line=0; line < bitmap->Height(); line++)
				{
					bitmap->GetLineData(((char*)bitmap_data)+bpl*line, line);
				}
			}
			else
			{
				memcpy(bitmap_data, source_data, data_size);
			}
			const_cast<OpBitmap*>(bitmap)->ReleasePointer(FALSE);
				
			CGDataProviderRef provider = CGDataProviderCreateWithData(bitmap_data, bitmap_data, data_size, DeleteBuffer);
			CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
			CGImageRef image = CGImageCreate(bitmap->Width(), bitmap->Height(), 8, 32, bpl, colorSpace, alpha, provider, NULL, true, kCGRenderingIntentAbsoluteColorimetric);
			
			if (pb && types)
			{
				CFMutableDataRef dataRef;
				CGImageDestinationRef imgDest;

				dataRef = CFDataCreateMutable(kCFAllocatorDefault, 0);
				imgDest = CGImageDestinationCreateWithData(dataRef, kUTTypeTIFF, 1, NULL);
				CGImageDestinationAddImage(imgDest, image, NULL);
				CGImageDestinationFinalize(imgDest);
				CFRelease(imgDest);
				[pb declareTypes:types owner:nil];
				[pb setData:[NSData dataWithBytes:CFDataGetMutableBytePtr(dataRef) length:CFDataGetLength(dataRef)] forType:NSTIFFPboardType];
				CFRelease(dataRef);
				
				m_owner_token = token;
				retval = OpStatus::OK;
			}
			
			CFRelease(provider);
			CGImageRelease(image);
		}
		CFRelease(colorSpace);
	}
	
	if (OpStatus::IsSuccess(retval))
		m_owner_token = token;
	
	return retval;
}

#ifdef DOCUMENT_EDIT_SUPPORT
BOOL CocoaOpClipboard::SupportHTML()
{
	return TRUE;
}

BOOL CocoaOpClipboard::HasTextHTML()
{
	NSPasteboard *pb = m_service_pboard ? m_service_pboard : [NSPasteboard generalPasteboard];
	NSArray *types = [NSArray arrayWithObjects: NSHTMLPboardType, NSStringPboardType, nil];
	if (pb && types)
	{
		NSString *bestType = [pb availableTypeFromArray:types];
		return (bestType != nil);
	}
	return FALSE;
}

OP_STATUS CocoaOpClipboard::PlaceTextHTML(const uni_char* htmltext, const uni_char* text, UINT32 token)
{
	if (htmltext)
	{
		NSPasteboard *pb = m_service_pboard ? m_service_pboard : [NSPasteboard generalPasteboard];
		if (m_service_types && ![m_service_types containsObject:NSStringPboardType])
			return OpStatus::OK;
		NSArray *types = text ? [NSArray arrayWithObjects: NSHTMLPboardType, NSStringPboardType, nil] : [NSArray arrayWithObjects: NSHTMLPboardType, nil];
		NSString *str = [NSString stringWithCharacters:(const unichar*)htmltext length:uni_strlen(htmltext)];
		if (pb && types && str)
		{
			[pb declareTypes:types owner:nil];
			[pb setString:str forType:NSHTMLPboardType];
			if (text)
				[pb setString:[NSString stringWithCharacters:(const unichar*)text length:uni_strlen(text)] forType:NSStringPboardType];
			m_owner_token = token;
			return OpStatus::OK;
		}
	}
	if (text) {
		return PlaceText(text, token);
	}
	return OpStatus::ERR;
}

OP_STATUS CocoaOpClipboard::GetTextHTML(OpString &text)
{
	NSPasteboard *pb = m_service_pboard ? m_service_pboard : [NSPasteboard generalPasteboard];
	NSArray *types = [NSArray arrayWithObjects: NSHTMLPboardType, NSStringPboardType, nil];
	if (pb && types)
	{
		NSString *bestType = [pb availableTypeFromArray:types];
		if (bestType)
		{
			NSString *str = [pb stringForType:bestType];
			OpString raw_clipboard;
			if (str && raw_clipboard.Reserve([str length] + 1))
			{
				[str getCharacters:(unichar*)raw_clipboard.CStr() range:NSMakeRange(0, [str length])];
				raw_clipboard.CStr()[[str length]] = '\0';
				
				StreamBuffer<uni_char> stripped_clipboard;
				RETURN_IF_ERROR(stripped_clipboard.Reserve(raw_clipboard.Length()));
				
				// use <BODY and </BODY> to include the most relevant formatting
				// we can't use <!--StartFragment--> because of Excel putting <table> before that
				int start = raw_clipboard.FindI(UNI_L("<BODY"));
				if (start != KNotFound && ((start = raw_clipboard.Find(UNI_L(">"), start)) != KNotFound))
				{
					int end_of_body = raw_clipboard.FindI(UNI_L("</BODY>"));
					if (end_of_body != KNotFound)
					{
						int end;
						while ((end = raw_clipboard.Find("<!", ++start)) != KNotFound && end < end_of_body)
						{
							RETURN_IF_ERROR(stripped_clipboard.Append(raw_clipboard.CStr() + start, end-start));
							start = raw_clipboard.Find(">", end);
							if (start == KNotFound)
								break;
						}

						if (start != KNotFound && start < end_of_body)
						{
							RETURN_IF_ERROR(stripped_clipboard.Append(raw_clipboard.CStr() + start, end_of_body-start));
						}

						StreamBufferToOpString(stripped_clipboard, text);
					}
				}
				else
				{
					text.Set(raw_clipboard.CStr());
				}

				return OpStatus::OK;
			}
		}
	}
	
	return OpStatus::ERR;
}
#endif // DOCUMENT_EDIT_SUPPORT

OP_STATUS CocoaOpClipboard::EmptyClipboard(UINT32 token)
{
	if (m_owner_token == token)
	{
		// Empty the clipboard
		NSPasteboard *pb = m_service_pboard ? m_service_pboard : [NSPasteboard generalPasteboard];
		NSArray *types = [NSArray arrayWithObjects: NSStringPboardType, nil];
		if (pb && types)
		{
			[pb declareTypes: types owner: pb];
			[pb setString: @"" forType: NSStringPboardType];
		}
	}
	return OpStatus::ERR;
}

void CocoaOpClipboard::SetServicePasteboard(NSPasteboard *pboard, NSArray *types)
{
	m_service_pboard = pboard;
	m_service_types = types;
}

#endif // NO_CARBON
