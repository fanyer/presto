/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "platforms/windows/pi/WindowsOpClipboard.h"
#include "modules/pi/OpBitmap.h"

OP_STATUS OpClipboard::Create(OpClipboard** new_opclipboard)
{
	OP_ASSERT(new_opclipboard != NULL);
	*new_opclipboard = new WindowsOpClipboard();
	RETURN_OOM_IF_NULL(new_opclipboard);

	return OpStatus::OK;
}

WindowsOpClipboard::WindowsOpClipboard()
{
	m_owner_token = 0;
	html_cf = RegisterClipboardFormat(UNI_L("HTML Format"));
}

WindowsOpClipboard::~WindowsOpClipboard()
{
}

BOOL WindowsOpClipboard::HasText()
{
	BOOL has_text = FALSE;
	if(OpenClipboard(NULL))
	{
		has_text = ::IsClipboardFormatAvailable(CF_UNICODETEXT) ||
					::IsClipboardFormatAvailable(CF_TEXT);
		CloseClipboard();
	}
	return has_text;
}

#ifdef DOCUMENT_EDIT_SUPPORT

BOOL WindowsOpClipboard::HasTextHTML()
{
	BOOL has_html = FALSE;
	if(OpenClipboard(NULL))
	{
		has_html = ::IsClipboardFormatAvailable(html_cf);
		CloseClipboard();
	}
	return has_html;
}

#endif // DOCUMENT_EDIT_SUPPORT

BOOL WindowsOpClipboard::PlaceTextOnClipboard(const uni_char* text, UINT uFormat, int bufsize)
{
	if (!text)
		return FALSE;

	BOOL ok = FALSE;
	HGLOBAL hClipboardData = NULL;

	if (CF_UNICODETEXT == uFormat)
	{
		UINT32 size = sizeof(uni_char) * ConvertLineBreaks(text, bufsize/sizeof(uni_char), NULL,0);
		hClipboardData = GlobalAlloc(GMEM_DDESHARE,size);
		if (hClipboardData)
		{
			uni_char* data = reinterpret_cast<uni_char*>(GlobalLock(hClipboardData));
			if (data)
			{
				ConvertLineBreaks(text, bufsize/sizeof(uni_char), data, size/sizeof(uni_char));
				GlobalUnlock(hClipboardData);
				HANDLE ret = SetClipboardData(uFormat, hClipboardData);
				ok = (ret != NULL);
			}
		}
	}
	else if (html_cf == uFormat)
	{
		OpString8 string_8;
		string_8.SetUTF8FromUTF16(text);

		OpString8 out;

		/* The start of the html text is at 97 characters,
           the start of the actual text to be put on the clipboard is
		   at 221.  If you change the any of the text below, you
		   must make sure that the numbers continue to be correct. */

		int start_html, start_fragment, end_html, end_fragment;
		start_html = 97;
		start_fragment = 221;
		end_html = 257 + string_8.Length();
		end_fragment = start_fragment + string_8.Length();

		out.AppendFormat("Version:1.0\r\nStartHTML:%08d\r\nEndHTML:%08d\r\nStartFragment:%08d\r\nEndFragment:%08d\r\n<html>\r\n<head>\r\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\r\n</head>\r\n<body>\r\n<!--StartFragment-->", start_html, end_html, start_fragment, end_fragment);
		out.Append(string_8.CStr());
		out.Append("<!--EndFragment-->\r\n</body>\r\n</html>");


		hClipboardData = GlobalAlloc(GMEM_DDESHARE, out.Length());
		if (hClipboardData)
		{
			char* data = reinterpret_cast<char*>(GlobalLock(hClipboardData));
			if (data)
			{
				memcpy(data, out.CStr(), out.Length());

				GlobalUnlock(hClipboardData);
				HANDLE ret = SetClipboardData(uFormat, hClipboardData);
				ok = (ret != NULL);
			}
		}

	}
	else
	{
		UINT32 size = ConvertLineBreaks(text, bufsize, NULL, 0);

		uni_char* buf = new uni_char[size];
		if(!buf)
			return OpStatus::ERR_NO_MEMORY;

		ConvertLineBreaks(text, bufsize, buf, size);

		int chars_req = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, (LPCWSTR)buf, size, NULL, 0, NULL, NULL);
		if (chars_req)
		{
			hClipboardData = GlobalAlloc(GMEM_DDESHARE, chars_req);
			if (hClipboardData)
			{
				char* data = reinterpret_cast<char*>(GlobalLock(hClipboardData));
				if (data)
				{
					WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, (LPCWSTR)buf, size, data, chars_req, NULL, NULL);

					GlobalUnlock(hClipboardData);
					HANDLE ret = SetClipboardData(uFormat, hClipboardData);
					ok = (ret != NULL);
				}
			}
		}
		delete[] buf;
		buf = NULL;
	}

	if (!ok && hClipboardData)
	{
		GlobalFree(hClipboardData);
	}

	return ok;
}


OP_STATUS WindowsOpClipboard::PlaceText(const uni_char* text, UINT32 token)
{
	if(!text)
	{
		return OpStatus::ERR_NULL_POINTER;
	}
	if(OpenClipboard(NULL))
	{
	    m_owner_token = token;
		
		::EmptyClipboard();
		// Win98 need both CF_TEXT to work (CF_UNICODETEXT won't affcet win98).
		PlaceTextOnClipboard(text, CF_TEXT, uni_strlen(text) + 1);
		PlaceTextOnClipboard(text, CF_UNICODETEXT, (uni_strlen(text) + 1) * sizeof(uni_char));

		CloseClipboard();
	}
	return OpStatus::OK;
}

#ifdef DOCUMENT_EDIT_SUPPORT

OP_STATUS WindowsOpClipboard::PlaceTextHTML(const uni_char* htmltext, const uni_char* text, UINT32 token)
{
	if(!text)
	{
		return OpStatus::ERR_NULL_POINTER;
	}
	if(OpenClipboard(NULL))
	{
	    m_owner_token = token;
		int text_length = uni_strlen(text);

		::EmptyClipboard();

		// Win98 need both CF_TEXT to work (CF_UNICODETEXT won't affcet win98).
		PlaceTextOnClipboard(text, CF_TEXT, text_length + 1);
		PlaceTextOnClipboard(text, CF_UNICODETEXT, (text_length + 1) * sizeof(uni_char));
		PlaceTextOnClipboard(htmltext, html_cf, (uni_strlen(htmltext) + 1) * sizeof(uni_char));

		CloseClipboard();
	}
	return OpStatus::OK;
}

#endif // DOCUMENT_EDIT_SUPPORT

BOOL WindowsOpClipboard::GetTextFromClipboard(OpString& string, UINT uFormat)
{
	BOOL ok = FALSE;
	HANDLE hClipboardData = GetClipboardData(uFormat);
	if (hClipboardData)
	{
		char* pchData = (char*) GlobalLock(hClipboardData);

		if (pchData)
		{
			size_t len;
			if (uFormat == CF_UNICODETEXT
				)
				len = uni_strlen((uni_char*)pchData);
			else
				len = strlen(pchData);

			uni_char* text = string.Reserve(len + 1);

			if (text)
			{
				if (uFormat == CF_UNICODETEXT)
					uni_strcpy(text, (uni_char*)pchData);
				else if (html_cf == uFormat)
				{
					string.SetFromUTF8(pchData);
				}
				else
				{
					MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, pchData, -1,
										text, (len + 1) * sizeof(uni_char));
				}

				ok = TRUE;
			}

			GlobalUnlock(hClipboardData);
		}
	}
	return ok;
}

OP_STATUS WindowsOpClipboard::GetText(OpString& text)
{
	if(HasText() && OpenClipboard(NULL))
	{
		BOOL ok = GetTextFromClipboard(text, CF_UNICODETEXT);
		if (!ok)
			ok = GetTextFromClipboard(text, CF_TEXT);

		CloseClipboard();

		if (!ok)
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

#ifdef DOCUMENT_EDIT_SUPPORT

OP_STATUS WindowsOpClipboard::GetTextHTML(OpString& text)
{
	if(HasTextHTML() && OpenClipboard(NULL))
	{
		OpString raw_clipboard;
		BOOL ok = GetTextFromClipboard(raw_clipboard, html_cf);

		CloseClipboard();

		if (!ok)
			return OpStatus::ERR_NO_MEMORY;

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
	}
	return OpStatus::OK;
}

#endif // DOCUMENT_EDIT_SUPPORT

OP_STATUS WindowsOpClipboard::PlaceBitmap(const class OpBitmap* bitmap, UINT32 token)
{
	if (bitmap && OpenClipboard(NULL))
	  {
		m_owner_token = token;

		UINT32 width = bitmap->Width();
		UINT32 height = bitmap->Height();
		UINT8 bpp = 32;  // always get bitmap data in 32 bit

		UINT32 header_size = sizeof(BITMAPINFOHEADER);
		BITMAPINFO bmpi;
		bmpi.bmiHeader.biSize = header_size;
		bmpi.bmiHeader.biWidth = width;
		bmpi.bmiHeader.biHeight = height;
		bmpi.bmiHeader.biPlanes = 1;
		bmpi.bmiHeader.biBitCount = bpp;
		bmpi.bmiHeader.biCompression = BI_RGB;
		bmpi.bmiHeader.biSizeImage = 0;
		bmpi.bmiHeader.biXPelsPerMeter = 0;
		bmpi.bmiHeader.biYPelsPerMeter = 0;
		bmpi.bmiHeader.biClrUsed = 0;
		bmpi.bmiHeader.biClrImportant = 0;

		UINT32 bytes_per_line = width * (bpp >> 3);

		// allocate some space for the bitmap
		// hGmem will be freed by Windows when this clipboard data is not used anymore
		UINT32 len = bytes_per_line * height + header_size;
		HANDLE hGmem = GlobalAlloc(GHND | GMEM_DDESHARE, len);
		if (!hGmem)
		{
			CloseClipboard();
			return OpStatus::ERR_NO_MEMORY; // not enough mem
		}
		LPSTR buffer = (LPSTR) GlobalLock(hGmem);
	
		memcpy(buffer, &bmpi, header_size);

		// write all bitmap data into the clipboard bitmap
		for (UINT32 line = 0; line < height; line++)
		{
			UINT8* dest = (UINT8*) &buffer[line * bytes_per_line + header_size];
			bitmap->GetLineData(dest, height - line - 1);
		}
		GlobalUnlock(hGmem);

		::EmptyClipboard();
		SetClipboardData(CF_DIB, hGmem);
		CloseClipboard();
	}
	return OpStatus::OK;
}

OP_STATUS WindowsOpClipboard::EmptyClipboard(UINT32 token)
{
    if (m_owner_token == token && OpenClipboard(NULL))
    {
        ::EmptyClipboard();
        ::CloseClipboard(); 
    }

    return OpStatus::OK;
}

UINT32 WindowsOpClipboard::ConvertLineBreaks(const uni_char* src, UINT32 src_size, uni_char* dst, UINT32 dst_size)
{
	if (dst == NULL)
		dst_size = 0;

	UINT32 req_size = 0;
	uni_char* s = dst;

	//Copy what can fit in the destination buffer
	UINT32 i;
	for(i = 0; i < src_size && (UINT32)(s - dst) < dst_size; i++)
	{
		switch (src[i])
		{
		case UNI_L('\r'):
			break;
		case UNI_L('\n'):
			*(s++) = UNI_L('\r');
			//fall-through, \n will be added after the \r
		default:
			*(s++) = src[i];
			break;
		}
	}

	req_size = s - dst;

	//Count how much bigger the buffer should be to fit the whole result
	for (;i < src_size;i++)
	{
		switch (src[i])
		{
		case UNI_L('\r'):
			break;
		case UNI_L('\n'):
			req_size++;
		default:
			req_size++;
			break;
		}
	}

	return req_size;
}

