/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "modules/util/opfile/opfile.h"
#include "modules/util/path.h"
#include "adjunct/widgetruntime/GadgetUtils.h"
#include "modules/pi/OpBitmap.h"
#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL

//
// This code is required to build an ICNS file out of raw bitmap
//
#define ICNS_128X128_32BIT_DATA       0x69743332  // "it32"
#define ICNS_128X128_8BIT_MASK        0x74386D6B  // "t8mk"

#define ICNS_48x48_32BIT_DATA         0x69683332  // "ih32"
#define ICNS_48x48_8BIT_MASK          0x68386D6B  // "h8mk"

#define ICNS_32x32_32BIT_DATA         0x696C3332  // "il32"
#define ICNS_32x32_8BIT_MASK          0x6C386D6B  // "l8mk"

#define ICNS_16x16_32BIT_DATA         0x69733332  // "is32"
#define ICNS_16x16_8BIT_MASK          0x73386D6B  // "s8mk"

#define ICNS_FILENAME				  "/icon.icns"

#if TARGET_CPU_PPC
#define PIXEL_SWAP(a) (a)
#else
#define PIXEL_SWAP(a) (((a)>>24)|(((a)>>8)&0xff00)|(((a)&0xff00)<<8)|(((a)&0xff)<<24))
#endif

typedef enum 
{
	ICNS_TYPE_UNKNOWN = 0,
	ICNS_TYPE_16X16,
	ICNS_TYPE_32X32,
	ICNS_TYPE_48X48,
	ICNS_TYPE_128X128
} IcnsType;

typedef struct
{
	int w;
	int h;
	int bytes_per_pixel;
	unsigned char *data;
} ImageData;

typedef struct
{
	IcnsType type;
	Handle   image;
	Handle   mask;
} IcnsData;

static int CreateIcns(ImageData *image, IcnsData *icnsImage)
{
	unsigned char* rawData = NULL;
	float oneOverAlpha;
	int size = (image->w > image->h) ? image->w : image->h;
	int bytesPerRow = image->w * image->bytes_per_pixel;
	int x,y;
	unsigned char* pSrc;
	unsigned char* pDest;
	unsigned char  alphaByte;
	int w_shift = 0;
	int h_shift = 0;
	
	switch (size)
	{
		case 16:  icnsImage->type = ICNS_TYPE_16X16; break;
		case 32:  icnsImage->type = ICNS_TYPE_32X32; break;	
		case 48:  icnsImage->type = ICNS_TYPE_48X48; break;	
		case 128: icnsImage->type = ICNS_TYPE_128X128; break;	
		default:  icnsImage->type = ICNS_TYPE_UNKNOWN; break;
	}
	
	// Now calculate shifts if it's not wide or high enough
	if (image->w < size) w_shift = (size - image->w) / 2;
	else if (image->h < size) h_shift = (size - image->h) / 2;
	
	if (icnsImage->type == ICNS_TYPE_UNKNOWN || (image->bytes_per_pixel != 4 && image->bytes_per_pixel != 3))
		return 0;
	
	// Now copy image data first...
	icnsImage->image = NewHandle(size * size * 4);
	rawData = (unsigned char*)(*(icnsImage->image));	
	
	pDest = rawData;
	pDest += (h_shift * image->w * 4);
	if (image->bytes_per_pixel == 4)
	{
		for (y = 0; y < image->h; y++) 
		{
			pSrc = image->data + y * bytesPerRow;
			pDest += w_shift * 4;
			for (x = 0; x < image->w ; x++) 
			{
					*pDest++ = alphaByte = *(pSrc+3);
					if (alphaByte) {
						oneOverAlpha = 255.0f / (float)alphaByte;
						*pDest++ = *(pSrc+0) * oneOverAlpha;
						*pDest++ = *(pSrc+1) * oneOverAlpha;
						*pDest++ = *(pSrc+2) * oneOverAlpha;
					} else {
						*pDest++ = 0;
						*pDest++ = 0;
						*pDest++ = 0;
					}
					pSrc+=4;
				}
				pDest += ((size - image->w) - w_shift) * 4;
			}
		}
		else
		{
			for (y = 0; y < image->h; y++) 
			{
				pSrc = image->data + y * bytesPerRow;
				pDest += w_shift * 4;
				for (x = 0; x < image->w; x++) 
				{
					*pDest++ = 0xFF;
					*pDest++ = *pSrc++;
					*pDest++ = *pSrc++;
					*pDest++ = *pSrc++;
				}
				pDest += ((size - image->w) - w_shift) * 4;
			}
		}
		
		
		// Now create a mask and copy alpha channel information byte by byte from original data
		icnsImage->mask = NewHandle(size * size);
		rawData = (unsigned char*)*icnsImage->mask;
		rawData += (h_shift * image->w);
		if (image->bytes_per_pixel == 4)
		{
			for (y = 0; y < image->h; y++)
			{
				rawData += w_shift;
				pSrc = image->data + y * bytesPerRow;
				for (x = 0; x < image->w; x++)
				{
					*rawData++ = *(pSrc+3);
					pSrc+=4;
				}
				rawData += ((size - image->w) - w_shift);
			}
		}
		else
		{
			for (y = 0; y < image->h; y++)
			{
				rawData += w_shift;
				for (x = 0; x < image->w; x++)
				{
					*rawData++ = 255;
				}
				rawData += ((size - image->w) - w_shift);
			}
		}
		
		return 1;
}
	
static int SaveAsIcns(IcnsData *image, OpString *path)
{
	IconFamilyHandle hIconFamily;
	OSErr result;
	OpFile fh;
	// Now - create the handle
	hIconFamily = (IconFamilyHandle) NewHandle( 0 );
	
	// Set what we can set
	switch (image->type)
	{
		case ICNS_TYPE_128X128:
			result = SetIconFamilyData(hIconFamily, kThumbnail32BitData, image->image);
			result = SetIconFamilyData(hIconFamily, kThumbnail8BitMask, image->mask);
			break;
				
		case ICNS_TYPE_48X48:
			result = SetIconFamilyData(hIconFamily, kHuge32BitData, image->image);
			result = SetIconFamilyData(hIconFamily, kHuge8BitMask, image->mask);
			break;
				
		case ICNS_TYPE_32X32:
			result = SetIconFamilyData(hIconFamily, kLarge32BitData, image->image);
			result = SetIconFamilyData(hIconFamily, kLarge8BitMask, image->mask);
			break;
				
		case ICNS_TYPE_16X16:
			result = SetIconFamilyData(hIconFamily, kSmall32BitData, image->image);
			result = SetIconFamilyData(hIconFamily, kSmall8BitMask, image->mask);
			break;
				
	}
		
	// And finally - save the content
	fh.Construct(path->CStr());
	fh.Open(OPFILE_WRITE);
	fh.Write(*hIconFamily, GetHandleSize((Handle)hIconFamily));
	fh.Close();
	
	DisposeHandle((Handle)hIconFamily);
	return 1;
}	


// FIXME: There is no DesktopShortcut::CreateIcon() anymore.

/** Create icon on the platform.
 * 
 * Platform implementation should do everything necessary to create icon from the specified
 * image file. It may include necessity of the source image convertion into appropriate 
 * icon format on the platform. String iconPath should contain path to icon created as result
 * of conversion.
 *
 * @param srcPath Path to the image source file
 * @param iconPath Path to created icon.
 * @return OK, ERR or ERR_NO_MEMORY
 */
OP_STATUS DesktopShortcut::CreateIcon(const OpString* srcPath, OpString* iconPath)
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	NSImage           *imgFile = [[NSImage alloc] initWithContentsOfFile:[NSString stringWithCharacters:(const unichar *)srcPath->CStr() length:srcPath->Length()]];
	OP_STATUS		   status = OpStatus::ERR;
	uni_char		  *path;
	ImageData		   imgData;
	NSImage           *scaledImgFile = nil;
	IcnsData		   icnsData;
	
	// We had a problem loading this icon. We need to use the default one then :/
	if (imgFile == nil)
	{
		Image iconImage;
		
		GadgetUtils::GetGenericGadgetIcon(iconImage, 128);
		
		OpBitmap *iconBitmap = iconImage.GetBitmap(NULL);
		if(!iconBitmap)
		{
			return OpStatus::ERR;
		}
		UINT32 *data = (UINT32 *)iconBitmap->GetPointer(OpBitmap::ACCESS_READONLY);
		
		NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **)NULL pixelsWide:128 pixelsHigh:128 bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO 
																	colorSpaceName:NSCalibratedRGBColorSpace bitmapFormat:NSAlphaFirstBitmapFormat
																	   bytesPerRow:(4*128) bitsPerPixel:32];
		
		if (rep)
		{
			for (int i = 0; i<128*128; i++)
			{
				UINT32 pixel = data[i];
				pixel = PIXEL_SWAP(pixel);
				((UINT32 *)[rep bitmapData])[i] = pixel;
			}
			
			imgFile = [[NSImage alloc] init];
			[imgFile addRepresentation:rep];
			[rep release];
		}
		
	}
	
	
	if (imgFile != nil)
	{
		// First - resize the image
		NSSize size = [imgFile size];
		NSSize origSize = size;
		
		int w = size.width;
		int h = size.height;
		int bVal = (w > h) ? w : h;
		int scaledImageSize = 16;
		
		if (bVal >= 128) scaledImageSize = 128;
		else if (bVal >= 48) scaledImageSize = 48;
		else if (bVal >= 32) scaledImageSize = 32;
		else scaledImageSize = 16;	
		
		float ratio = (float)scaledImageSize / (float)bVal ;
		
		w = floor(w * ratio);
		h = floor(h * ratio);

		size.width = w;
		size.height = h;
		
		scaledImgFile = [[NSImage alloc] initWithSize:size];
		
		[scaledImgFile lockFocus];
		[imgFile drawInRect: NSMakeRect(0, 0, w, h) fromRect: NSMakeRect(0, 0, origSize.width, origSize.height) operation: NSCompositeSourceOver fraction: 1.0];
		[scaledImgFile unlockFocus];
		
		NSBitmapImageRep *rep = [NSBitmapImageRep imageRepWithData:[scaledImgFile TIFFRepresentation]];
		if (rep != nil)
		{
			imgData.w = w;
			imgData.h = h;
			imgData.bytes_per_pixel = [rep bitsPerPixel] / 8;
			imgData.data = [rep bitmapData];

			CreateIcns(&imgData, &icnsData);
		
			// Time to save
			path = (uni_char *)malloc(sizeof(uni_char)*(srcPath->Length()+1));
			memcpy(path, srcPath->CStr(), sizeof(uni_char)*srcPath->Length()+1);
			OpPathRemoveFileName(path);
			iconPath->Set(path);
			iconPath->Append(ICNS_FILENAME);
			
			
			SaveAsIcns(&icnsData, iconPath);
			free(path);
			status = OpStatus::OK;
			
		}
		[imgFile release];
		[scaledImgFile release];
	}
	
	[pool release];
	
	return status;
}

#endif //WIDGET_RUNTIME_SUPPORT
