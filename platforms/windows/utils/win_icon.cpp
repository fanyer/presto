/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#include "core/pch.h"

#include "platforms/windows/utils/win_icon.h"

#include "adjunct/desktop_pi/desktop_pi_util.h"

#include "modules/img/image.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url_man.h"

#include "platforms/windows/win_handy.h"
#include "platforms/windows/utils/com_helpers.h"

#include <olectl.h>

HBITMAP CreateHBITMAPFromOpBitmap(OpBitmap *bitmap)
{
	UINT32* tdata;
	BITMAPINFO bi;
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = bitmap->Width();
	bi.bmiHeader.biHeight = -(int)bitmap->Height();;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = 0;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	HBITMAP bm = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&tdata, NULL, 0);

	if(bm)
	{
		UINT32* data = (UINT32*)bitmap->GetPointer(OpBitmap::ACCESS_READONLY);
		if(data)
		{
			op_memcpy(tdata, data, bitmap->Width() * bitmap->Height() * 4);
	
			bitmap->ReleasePointer();
		}
	}
	return bm;
}

HICON IconUtils::GetAsIcon(const OpBitmap* bitmap)
{
	HICON icon = NULL;
	ICONINFO info;
	UINT32 height = bitmap->Height();
	UINT32 width = bitmap->Width();

	ZeroMemory(&info, sizeof(info));
	info.fIcon = TRUE;

	UINT32* tdata = NULL;
	BITMAPV5HEADER bi;

	ZeroMemory(&bi, sizeof(BITMAPV5HEADER));

	bi.bV5Size			= sizeof(BITMAPV5HEADER);
	bi.bV5Width			= width;
	bi.bV5Height		= -(int)height;
	bi.bV5Planes		= 1;
	bi.bV5Compression	= BI_RGB;

	// The following mask specification specifies a supported 32 BPP ARGB format
	bi.bV5BitCount		= 32;
	bi.bV5CSType		= LCS_WINDOWS_COLOR_SPACE;	// system color space
	bi.bV5Intent		= LCS_GM_IMAGES;

	HDC dc  = GetDC(NULL);

	HBITMAP bm = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS, reinterpret_cast<void**>(&tdata), NULL, 0);

	ReleaseDC(NULL, dc);

	if(bm)
	{
		UINT32 bytes_per_line = width * 4;
		UINT8* dest = (UINT8*)tdata;
		UINT8* linedata;

		for (UINT32 line = 0; line < height; line++)
		{
			linedata = &dest[line * bytes_per_line];

			bitmap->GetLineData(linedata, line);
		}
		BOOL has_alpha = bitmap->HasAlpha();

		OpAutoArray<UINT8> mask_bits;

		// Even if all our bitmaps has an alpha channel, Windows might not agree when all alpha values
		// are zero. So the monochrome bitmap is created with all pixels transparent
		// for this case. Otherwise, it is created with all pixels opaque.
		if(!has_alpha)
		{
			int mask_size = bytes_per_line * height;
			mask_bits.reset(OP_NEWA(UINT8, (mask_size)));

			// make all pixels transparent
			op_memset(mask_bits.get(), 0xFF, mask_size);
		}

		// Icons are generally created using an AND and XOR masks where the AND
		// specifies boolean transparency (the pixel is either opaque or
		// transparent) and the XOR mask contains the actual image pixels. However,
		// since our bitmap has an alpha channel, the AND monochrome bitmap won't
		// actually be used for computing the pixel transparency. Since every icon
		// must have an AND mask bitmap, we go ahead and create one so that we can
		// associate it with the ICONINFO structure we'll later pass to
		// ::CreateIconIndirect(). The monochrome bitmap is created such that all the
		// pixels are opaque.

		info.hbmMask = CreateBitmap(bitmap->Width(), bitmap->Height(), 1, 1, reinterpret_cast<void *>(mask_bits.get()));
		info.hbmColor = bm;

		icon = CreateIconIndirect(&info);

		if(info.hbmMask)
		{
			DeleteObject(info.hbmMask);
		}
		if(info.hbmColor)
		{
			DeleteObject(info.hbmColor);
		}
	}
	return icon;
}

OP_STATUS IconUtils::WriteIcoFile(Image src_image, OpStringC dest_file_path)
{
	if (src_image.IsEmpty() || src_image.IsFailed())
	{
		return OpStatus::ERR;
	}

	OpBitmap* icon_bitmap = src_image.GetBitmap(null_image_listener);
	RETURN_OOM_IF_NULL(icon_bitmap);

	OP_STATUS s = IconUtils::SaveOpBitmapAsIco(*icon_bitmap, dest_file_path);

	src_image.ReleaseBitmap();

	return s;
}

OP_STATUS IconUtils::SaveOpBitmapAsIco(const OpBitmap& bmp, OpStringC outFile)
{
	const BYTE bpp = MAX(4, bmp.GetBpp());

	// Must be a multiple of 4.
	UINT32 bytes_per_line_XOR = bmp.Width() * bpp / 8;
	bytes_per_line_XOR = (bytes_per_line_XOR & 0xFFFFFFFC)
		+ (0 == (bytes_per_line_XOR & 0x00000003) ? 0 : 4);

	// Must be a multiple of 4.
	UINT32 bytes_per_line_AND = bmp.Width() / 8;
	bytes_per_line_AND = (bytes_per_line_AND & 0xFFFFFFFC)
		+ (0 == (bytes_per_line_AND & 0x00000003) ? 0 : 4);

	const UINT32 color_table_size =
		bpp <= 8 ? sizeof(RGBQUAD) * (1U << bpp) : 0;

	ICONDIR icon_dir = {};
	icon_dir.reserved = 0;
	icon_dir.type = 1;
	icon_dir.count = 1;

	ICONDIRENTRY icon_dir_entry = {};
	icon_dir_entry.width = bmp.Width();
	icon_dir_entry.height = bmp.Height();
	icon_dir_entry.color_count = bpp >= 8 ? 0 : (1U << bpp);
	icon_dir_entry.reserved = 0;
	icon_dir_entry.color_planes = 1;
	icon_dir_entry.bits_per_pixel = bpp;
	icon_dir_entry.size =
		sizeof(BITMAPINFOHEADER)
		// Color table?
		+ color_table_size
		// XOR mask
		+ bytes_per_line_XOR * bmp.Height()
		// AND mask
		+ bytes_per_line_AND * bmp.Height();
	icon_dir_entry.data_offset = sizeof(ICONDIR) + sizeof(ICONDIRENTRY);

	OpAutoHICON hIcon(IconUtils::GetAsIcon(&bmp));
	ICONINFO icon_info;
	if (!GetIconInfo(hIcon, &icon_info))
	{
		return OpStatus::ERR;
	}

	// Fill the BITMAPINFO structure with bitmap information.
	OpByteBuffer bmp_info_buffer;
	RETURN_IF_ERROR(
		bmp_info_buffer.Init(sizeof(BITMAPINFOHEADER) + color_table_size));
	op_memset(bmp_info_buffer.Buffer(), 0, bmp_info_buffer.BufferSize());

	BITMAPINFO* bmp_info =
		reinterpret_cast<BITMAPINFO*>(bmp_info_buffer.Buffer());
	bmp_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmp_info->bmiHeader.biWidth = bmp.Width();
	bmp_info->bmiHeader.biHeight = bmp.Height();
	bmp_info->bmiHeader.biPlanes = 1;
	bmp_info->bmiHeader.biBitCount = bpp;
	bmp_info->bmiHeader.biCompression = BI_RGB;

	// Get the XOR mask bits.
	OpByteBuffer xor_buffer;
	RETURN_IF_ERROR(xor_buffer.Init(bytes_per_line_XOR * bmp.Height()));
	HDC dc = CreateCompatibleDC(NULL);
	const int get_bits_result = GetDIBits(dc, icon_info.hbmColor,
		0, bmp.Height(), xor_buffer.Buffer(), bmp_info, DIB_RGB_COLORS);
	DeleteDC(dc);
	DeleteObject(icon_info.hbmColor);
	DeleteObject(icon_info.hbmMask);
	if (!get_bits_result)
	{
		return OpStatus::ERR;
	}

	// Start writing the ICO file.
	OpFile icoOutput;
	RETURN_IF_ERROR(icoOutput.Construct(outFile));
	RETURN_IF_ERROR(icoOutput.Open(OPFILE_WRITE));

	// Write the icon directory.
	RETURN_IF_ERROR(icoOutput.Write(&icon_dir, sizeof(icon_dir)));
	RETURN_IF_ERROR(icoOutput.Write(&icon_dir_entry, sizeof(icon_dir_entry)));

	// Write the bitmap header.
	// The combined height of the XOR and AND masks:
	bmp_info->bmiHeader.biHeight *= 2;
	RETURN_IF_ERROR(icoOutput.Write(
		bmp_info_buffer.Buffer(), sizeof(BITMAPINFOHEADER)));

	// Write the color table (?).
	if (color_table_size > 0)
	{
		RETURN_IF_ERROR(icoOutput.Write(bmp_info->bmiColors, color_table_size));
	}

	// Write the XOR (color) mask.
	RETURN_IF_ERROR(
		icoOutput.Write(xor_buffer.Buffer(), xor_buffer.BufferSize()));

	// Write the AND (monochrome, 1bpp) mask.
	const UINT32 and_mask = 0;
	for (UINT32 i = 0;
		i < bytes_per_line_AND * bmp.Height() / sizeof(and_mask); ++i)
	{
		RETURN_IF_ERROR(icoOutput.Write(&and_mask, sizeof(and_mask)));
	}
	return OpStatus::OK;
}

static unsigned long icon_counter = 0;

URL GetAttachmentIconURL(URL &base_url, const OpStringC& filename, BOOL big_attachment_icon)
{
	uni_char *tempurl = (uni_char *) g_memory_manager->GetTempBuf2k();

	uni_snprintf(tempurl, UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen())-1, UNI_L("attachment:/icon/%lu.bmp"), ++ icon_counter);
	URL icon = urlManager->GetURL(base_url, tempurl, (uni_char *) NULL);

	if (icon.IsEmpty())
		return icon;

	SHFILEINFO sfi;
	UINT uflags = SHGFI_ICON | (big_attachment_icon ? SHGFI_LARGEICON : SHGFI_SMALLICON);

	if (SHGetFileInfo(filename, 0, &sfi, sizeof(sfi), uflags))
	{
		HICON hIcon = sfi.hIcon;
		if (hIcon)
		{
			HDC hdc = CreateCompatibleDC(NULL);
			HBITMAP bmap;
			int cx, cy;

			cx = cy = big_attachment_icon ? 32 : 16;

			bmap = CreateScreenCompatibleBitmap( cx, cy);

			if (hdc && bmap)
			{
				RECT rc = {0,0,cx,cy};
				HBITMAP oldbmap = (HBITMAP)SelectObject( hdc, bmap);
				HBRUSH brush = GetSysColorBrush(COLOR_WINDOW);
				FillRect(hdc, &rc, brush);

				DrawIconEx(hdc, 0, 0, hIcon, cx, cy, 0, NULL, DI_NORMAL);

				HANDLE hDIB = CreateDIBFromDDB(bmap, BI_RGB, NULL);

				BITMAPFILEHEADER	hdr;
				LPBITMAPINFOHEADER	lpbi;
				
				if (!hDIB)
				{
					return icon;
				}

				icon.SetAttribute(URL::KForceContentType, URL_BMP_CONTENT);
				icon.ForceStatus(URL_LOADING);
				
				lpbi = (LPBITMAPINFOHEADER) ::GlobalLock(hDIB);;
				
				int nColors = 1 << lpbi->biBitCount;
				if (nColors > 256)
					nColors = 0;
				
				// Fill in the fields of the file header
				hdr.bfType		= ((WORD) ('M' << 8) | 'B');	// is always "BM"
				hdr.bfSize		= GlobalSize (hDIB) + sizeof( hdr );
				hdr.bfReserved1 	= 0;
				hdr.bfReserved2 	= 0;
				hdr.bfOffBits		= (DWORD) (sizeof( hdr ) + lpbi->biSize +
					nColors * sizeof(RGBQUAD));
				
				// Write the file header
				icon.WriteDocumentData(URL::KNormal, (const char *) &hdr, sizeof(hdr));
				
				// Write the DIB header and the bits
				icon.WriteDocumentData(URL::KNormal, (const char *) lpbi, GlobalSize(hDIB));
				icon.WriteDocumentDataFinished();
				icon.ForceStatus(URL_LOADED);

				::GlobalUnlock(hDIB);
				::GlobalFree(hDIB);

				SelectObject( hdc, oldbmap);

				DeleteDC(hdc);
				DeleteObject(bmap);

			}
		}
	}
	else
		icon = URL();

	return icon;
}


OP_STATUS IconUtils::CreateIconBitmap(OpBitmap **new_opbitmap, HICON icn)
{
	ICONINFO	iconInfo={0,};
	INT			result;
	BITMAPINFO	colorbinfo = {sizeof(BITMAPINFOHEADER), };

	HDC hBitmapDC = CreateCompatibleDC(NULL);
	if (!hBitmapDC)
	{
		*new_opbitmap = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	result = GetIconInfo(icn, &iconInfo);
	if (result)
	{
		result = GetDIBits(hBitmapDC, iconInfo.hbmColor, 0, 0, 0, &colorbinfo, DIB_RGB_COLORS);
	}

	//Failed case.
	if(!result)
	{
		colorbinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		colorbinfo.bmiHeader.biWidth = colorbinfo.bmiHeader.biHeight = 16;
		colorbinfo.bmiHeader.biBitCount= 32;
		colorbinfo.bmiHeader.biSizeImage = 0;
	}

	OP_STATUS ret = OpBitmap::Create(new_opbitmap, colorbinfo.bmiHeader.biWidth, colorbinfo.bmiHeader.biHeight, FALSE, TRUE);
	if ( OpStatus::IsError(ret) )
	{
		*new_opbitmap = NULL;
		DeleteDC(hBitmapDC);
		return ret;
	}

	BITMAPINFO bi=colorbinfo;
	bi.bmiHeader.biHeight = -bi.bmiHeader.biHeight;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biBitCount= 32;
	bi.bmiHeader.biPlanes= 1;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	LONG imageSize =  (colorbinfo.bmiHeader.biWidth * colorbinfo.bmiHeader.biHeight * 32) / 8;

	//0 index is for Black and 1 is for White
	UINT32 *icon_data[2]={0};
	HBITMAP bitmap[2];
	for (int i = 0 ; i < 2 ; i++)
	{
		bitmap[i] = CreateDIBSection(hBitmapDC, &bi, DIB_RGB_COLORS, (void**)&icon_data[i], NULL, 0);
		if (!bitmap[i])
		{
			OP_DELETE(*new_opbitmap);
			*new_opbitmap = NULL;
			DeleteDC(hBitmapDC);
			if (i==1)
				DeleteObject(bitmap[i-1]);
			return OpStatus::ERR_NO_MEMORY;
		}

		op_memset(icon_data[i], i*0xFF, imageSize);
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(hBitmapDC, bitmap[i]);
		DrawIconEx(hBitmapDC, 0, 0, icn, colorbinfo.bmiHeader.biWidth, colorbinfo.bmiHeader.biHeight, 0, NULL, DI_NORMAL);
		SelectObject(hBitmapDC, hOldBitmap);
	}

	//Create Alpha
	for ( int y = 0 ; y < colorbinfo.bmiHeader.biHeight ; y ++ )
	{
		UINT32 pixels_covered_sofar = colorbinfo.bmiHeader.biHeight * y;

		for ( int x = 0; x < colorbinfo.bmiHeader.biWidth; x ++ )
		{
			UINT32 blue_from_black_bitmap = (*(icon_data[0]+x+pixels_covered_sofar) & 0x000000FF);
			UINT32 blue_from_white_bitmap = (*(icon_data[1]+x+pixels_covered_sofar) & 0x000000FF);
			UINT32 generated_alpha  = (0x000000FF - (blue_from_white_bitmap - blue_from_black_bitmap)) << 24 ;
			*(icon_data[0]+x+pixels_covered_sofar) =  ( *(icon_data[0]+x+pixels_covered_sofar) & 0x00FFFFFF ) + generated_alpha;
		}
		(*new_opbitmap)->AddLine(icon_data[0]+colorbinfo.bmiHeader.biHeight*y, y);
	}

	//Free GDI reasource
	DeleteObject(bitmap[0]);
	DeleteObject(bitmap[1]);
	DeleteDC(hBitmapDC);
	if(iconInfo.hbmColor)
	{
		DeleteObject(iconInfo.hbmColor);
	}
	if(iconInfo.hbmMask)
	{
		DeleteObject(iconInfo.hbmMask);
	}
	return OpStatus::OK;
}

OP_STATUS IconUtils::CreateIconBitmap(OpBitmap **new_opbitmap, const uni_char* file_path, INT32 icon_size)
{
	OP_STATUS result = OpStatus::ERR;

	SHFILEINFO sfi;
	UINT uflags = SHGFI_ICON | (icon_size<24?SHGFI_SMALLICON:icon_size<40?SHGFI_LARGEICON:SHGFI_SHELLICONSIZE);

	//Must initialize Component Object Model (COM) with CoInitialize or OleInitialize prior to calling SHGetFileInfo.
	//Make sure either of the 3 MS APIs viz CoInitialize() or CoInitializeEx() or OleInitialize() has been called with COINIT_APARTMENTTHREADED earlier in the project.
	if (SHGetFileInfo(file_path, 0, &sfi, sizeof(sfi), uflags))
	{
		result = CreateIconBitmap(new_opbitmap, sfi.hIcon);
		DestroyIcon(sfi.hIcon); //Free GDI object.
	}
	return result;
}

OP_STATUS DesktopPiUtil::CreateIconBitmapForFile(OpBitmap **new_opbitmap, const uni_char* icon_path)
{
	OP_ASSERT(new_opbitmap != NULL);

	return IconUtils::CreateIconBitmap(new_opbitmap, icon_path, 16);
}
