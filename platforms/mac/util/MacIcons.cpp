/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpPainter.h"
#include "modules/logdoc/urlimgcontprov.h"

// Mac includes
#include "platforms/mac/util/MacIcons.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/CocoaVegaDefines.h"

static unsigned long icon_counter = 0;

UINT32 swap32(UINT32 val)
{
#ifdef OPERA_BIG_ENDIAN
	char *v = (char *)&val;
	UINT32 vv = (v[3] << 24) | (v[2] << 16) | (v[1] << 8) | v[0];
	return vv;
#else
	return val;
#endif
}

UINT16 swap16(UINT16 val)
{
#ifdef OPERA_BIG_ENDIAN
	char *v = (char *)&val;
	UINT16 vv = (v[1] << 8) | v[0];
	return vv;
#else
	return val;
#endif
}

URL GetAttachmentIconURL(URL &base_url, const OpStringC& filename, BOOL big_attachment_icon)
{
	big_attachment_icon = true;

	uni_char *tempurl = (uni_char *) g_memory_manager->GetTempBuf2k();

	uni_snprintf(tempurl, UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen())-1, UNI_L("attachment:/icon/%lu.bmp"), ++ icon_counter);
	URL icon = urlManager->GetURL(base_url, tempurl, (uni_char *) NULL);

	if (icon.IsEmpty())
		return icon;

	OpBitmap *bmp = GetAttachmentIconOpBitmap(filename, big_attachment_icon);

	if(!bmp)
	{
		return URL();
	}

	int w,h;
	w = bmp->Width();
	h = bmp->Height();

	int imageSize = w * h * 3;

	UINT32 biHeaderSize = 9*sizeof(UINT32) + 2*sizeof(UINT16);
	UINT32 bfHeaderSize = 3*sizeof(UINT16) + 2*sizeof(UINT32);

#ifdef OPERA_BIG_ENDIAN
	UINT16 bfType 		= ('B' << 8) | 'M';
#else
	UINT16 bfType 		= ('M' << 8) | 'B';
#endif
	UINT32 bfSize		= swap32(bfHeaderSize + biHeaderSize + sizeof(UINT32) + imageSize);
	UINT16 bfReserved1 	= 0;
	UINT16 bfReserved2 	= 0;
	UINT32 bfOffBits	= swap32(bfHeaderSize + biHeaderSize + sizeof(UINT32));

	UINT32 biSize = swap32(biHeaderSize);
	UINT32 biWidth = swap32(w);
	UINT32 biHeight = swap32(h);
	UINT16 biPlanes = swap16(1);
	UINT16 biBitCount = swap16(24);
	UINT32 biCompression = 0;
	UINT32 biSizeImage = 0;
	UINT32 biXPelsPerMeter = 0;
	UINT32 biYPelsPerMeter = 0;
	UINT32 biClrUsed = 0;
	UINT32 biClrImportant = 0;

	UINT32 rgbquad = 0;

	icon.SetAttribute(URL::KForceContentType, URL_BMP_CONTENT);
	icon.ForceStatus(URL_LOADING);

	icon.WriteDocumentData(URL::KNormal, (const char *) &bfType, sizeof(bfType));
	icon.WriteDocumentData(URL::KNormal, (const char *) &bfSize, sizeof(bfSize));
	icon.WriteDocumentData(URL::KNormal, (const char *) &bfReserved1, sizeof(bfReserved1));
	icon.WriteDocumentData(URL::KNormal, (const char *) &bfReserved2, sizeof(bfReserved2));
	icon.WriteDocumentData(URL::KNormal, (const char *) &bfOffBits, sizeof(bfOffBits));

	icon.WriteDocumentData(URL::KNormal, (const char *) &biSize, sizeof(biSize));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biWidth, sizeof(biWidth));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biHeight, sizeof(biHeight));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biPlanes, sizeof(biPlanes));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biBitCount, sizeof(biBitCount));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biCompression, sizeof(biCompression));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biSizeImage, sizeof(biSizeImage));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biXPelsPerMeter, sizeof(biXPelsPerMeter));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biYPelsPerMeter, sizeof(biYPelsPerMeter));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biClrUsed, sizeof(biClrUsed));
	icon.WriteDocumentData(URL::KNormal, (const char *) &biClrImportant, sizeof(biClrImportant));

	icon.WriteDocumentData(URL::KNormal, (const char *) &rgbquad, sizeof(rgbquad));

	unsigned char *data = new unsigned char[4*w];

	for(int i=0; i<h; i++)
	{
		memset(data,0,4*w);
		bmp->GetLineData(data,h-i-1);
		for(int j=0; j<w; j++)
		{
			unsigned char *color = data + 4*j;

#ifdef OPERA_BIG_ENDIAN
			unsigned char r = color[3];
			unsigned char g = color[2];
			unsigned char b = color[1];
			unsigned char a = color[0];
#else
			unsigned char r = color[0];
			unsigned char g = color[1];
			unsigned char b = color[2];
			unsigned char a = color[3];
#endif

			// assume white background

			r = (a*r + (255-a)*255) / 255;
			g = (a*g + (255-a)*255) / 255;
			b = (a*b + (255-a)*255) / 255;

			icon.WriteDocumentData(URL::KNormal, (const char *) &r, sizeof(r));
			icon.WriteDocumentData(URL::KNormal, (const char *) &g, sizeof(g));
			icon.WriteDocumentData(URL::KNormal, (const char *) &b, sizeof(b));
		}
	}

	delete [] data;

	icon.WriteDocumentDataFinished();
	icon.ForceStatus(URL_LOADED);

	return icon;
}

OpBitmap* GetAttachmentIconOpBitmap(const uni_char* filename, BOOL big_attachment_icon, int iSize)
{
	int 		cx, cy;
	OpBitmap* 	result = 0;
	SInt16		label;
	IconRef		iconRef;

	FSRef fsref;
	if(OpFileUtils::ConvertUniPathToFSRef(filename, fsref))
	{
		FSCatalogInfo 	info;

		if(noErr == FSGetCatalogInfo(&fsref, kIconServicesCatalogInfoMask, &info, NULL, NULL, NULL))
		{
			if(noErr == GetIconRefFromFileInfo(&fsref, 0, NULL, kIconServicesCatalogInfoMask, &info, kIconServicesNormalUsageFlag, &iconRef, &label))
			{
				if (!iSize)
					cx = cy = big_attachment_icon ? 32 : 16;
				else
					cx = cy = iSize;

				result = CreateOpBitmapFromIcon(iconRef, cx, cy);
				ReleaseIconRef(iconRef);
			}
		}
	}

	return result;
}

OpBitmap* CreateOpBitmapFromIcon(IconRef iconin, int cx, int cy)
{
	if (iconin)
	{
		OpBitmap* bm;
		if (OpStatus::IsSuccess(OpBitmap::Create(&bm, cx, cy, FALSE, TRUE, 0, 0, TRUE)))
		{
			CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
			void* p = bm->GetPointer(OpBitmap::ACCESS_WRITEONLY);
			memset(p, 0, bm->Height()*bm->GetBytesPerLine());
			CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
			CGContextRef ctx = 	CGBitmapContextCreate(p, bm->Width(), bm->Height(), 8, bm->GetBytesPerLine(), colorSpace, alpha);
			CGColorSpaceRelease(colorSpace);
			CGRect cgrect = CGRectMake(0,0,cx,cy);
			PlotIconRefInContext(ctx, &cgrect, kAlignAbsoluteCenter, kTransformNone, NULL, kPlotIconRefNormalFlags, iconin);
			CGContextFlush(ctx);
			CGContextRelease(ctx);
			bm->ReleasePointer();
			return bm;
		}
	}
	return NULL;
}

/**
* Note that this method will return the background icon only if the badge couldn't be applied.
*/
OpBitmap* CreateOpBitmapFromIconWithBadge(IconRef backgroundIcon, int cx, int cy, OpBitmap* badgeBitmap, OpRect &badgeDestRect)
{
	OpBitmap *theIcon = CreateOpBitmapFromIcon(backgroundIcon, cx, cy);
	if(theIcon && badgeBitmap)
	{
		OpPainter *panther = theIcon->GetPainter();
		if(panther)
		{
			panther->DrawBitmapScaledAlpha(badgeBitmap, OpRect(0,0,badgeBitmap->Width(),badgeBitmap->Height()), badgeDestRect);

			theIcon->ReleasePainter();
		}
	}

	return theIcon;
}

IconRef GetOperaApplicationIcon()
{
	OpFile 		iconFile;
	IconRef 	operaIcon = 0;

	iconFile.Construct(UNI_L("Opera.icns"), OPFILE_RESOURCES_FOLDER);
	BOOL exists = FALSE;
	iconFile.Exists(exists);
	if(exists)
	{
		FSRef fsref;
		if(OpFileUtils::ConvertUniPathToFSRef(iconFile.GetFullPath(), fsref))
		{
			if(noErr == RegisterIconRefFromFSRef('OPRA', 'APPL', &fsref, &operaIcon))
			{
				return operaIcon;
			}
		}
	}

	return 0;
}

IconRef	GetOperaDocumentIcon()
{
	OpFile 		iconDocsFile;
	IconRef 	docIcon = 0;

	iconDocsFile.Construct(UNI_L("OperaDocs.icns"), OPFILE_RESOURCES_FOLDER);
	BOOL exists = FALSE;
	iconDocsFile.Exists(exists);
	if(exists)
	{
		FSRef fsref;
		if(OpFileUtils::ConvertUniPathToFSRef(iconDocsFile.GetFullPath(), fsref))
		{
			if(noErr == RegisterIconRefFromFSRef('OPRA', 'HTML', &fsref, &docIcon))
			{
				return docIcon;
			}
		}
	}

	return 0;
}
