/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _PRINT_SUPPORT_

#include "modules/display/prn_dev.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"

#include "modules/pi/OpPrinterController.h"
#include "modules/pi/OpScreenInfo.h"

PrintDevice::PrintDevice()
{
	printer_controller = NULL;
}

PrintDevice::PrintDevice(DocumentManager* doc_man)
{
	doc_manager = doc_man;
	printer_controller = NULL;
}

void
PrintDevice::GetUserMargins(int& left, int& right, int& top, int& bottom) const
{
   	int left_margin, right_margin, top_margin, bottom_margin;
	UINT32 horizontal_dpi;
	UINT32 vertical_dpi;

	printer_controller->GetDPI(&horizontal_dpi, &vertical_dpi);

	int scale = printer_controller->GetScale();

	// Set margins according to prefsManager
	left_margin  = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginLeft);
	right_margin = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginRight);
	top_margin   = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginTop);
	bottom_margin= g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginBottom);

	left = int((long)left_margin * horizontal_dpi * 100 / 254 / scale);
	right = int((long)right_margin * horizontal_dpi * 100 / 254 / scale);
	top = int((long)top_margin * vertical_dpi * 100 / 254 / scale);
	bottom = int((long)bottom_margin * vertical_dpi * 100 / 254 / scale);
}

OP_STATUS
PrintDevice::Init(OpPrinterController* printerController)
{
	if (printer_controller)
		return OpStatus::OK;

	if (!printerController)
		return OpStatus::ERR;
	
	printer_controller = printerController;
	painter = (OpPainter*) printerController->GetPainter();

	return OpStatus::OK;
}

PrintDevice::~PrintDevice()
{
	OP_DELETE(printer_controller);
}

void PrintDevice::DrawBgColor(const RECT& rect)
{
	if (g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrintBackground))
		VisualDevice::DrawBgColor(rect);
}

BOOL PrintDevice::NewPage()
{
	if (!printer_controller)
	{
		return FALSE;
	}

	
/* JIMMED	if (printer_controller->EndPage())
	{
		return printer_controller->BeginPage();
	}
	else 
	{
		return FALSE;
	}*/

	OP_NEW_DBG("PrintDevice::NewPage()", "printing");

	printer_controller->EndPage();
	printer_controller->BeginPage();

	return TRUE; // FIXME!
}

BOOL PrintDevice::StartPage()
{
	return NewPage();
}

int PrintDevice::GetRenderingViewWidth() const
{
#if defined (GENERIC_PRINTING)	
	return GetPaperWidth() - GetPaperLeftOffset() - GetPaperRightOffset();	
#else
	return GetPaperWidth() - GetPaperLeftOffset() * 2;
#endif
}

int PrintDevice::GetRenderingViewHeight() const
{
#if defined (GENERIC_PRINTING)	
	return GetPaperHeight() - GetPaperTopOffset() - GetPaperBottomOffset();	
#else
	return GetPaperHeight() - GetPaperTopOffset() * 2;
#endif
}

void PrintDevice::GetDPI(UINT32* horizontal, UINT32* vertical)
{
	//FIXME:
	//Should get DPI from printer!
	VisualDevice::GetDPI(horizontal, vertical);
}

BOOL PrintDevice::IsLandscape()
{
	if (printer_controller)
	{
		UINT32 width = 0;
		UINT32 height = 0;

		printer_controller->GetSize(&width, &height);

		return width > height;
	}
	else
		return FALSE;
}

int PrintDevice::GetPaperWidth() const
{
	if (printer_controller)
	{
		UINT32 width = 0;
		UINT32 height = 0;

		printer_controller->GetSize(&width, &height);

		return width;
	}
	else
		return 0;
}

int PrintDevice::GetPaperHeight() const
{
	if (printer_controller)
	{
		UINT32 width = 0;
		UINT32 height = 0;

		printer_controller->GetSize(&width, &height);

		return height;
	}
	else
		return 0;
}

int PrintDevice::GetPaperLeftOffset() const
{
	if (printer_controller)
	{
		UINT32 left = 0;
		UINT32 top = 0;
		UINT32 right = 0;
		UINT32 bottom = 0;

		printer_controller->GetOffsets(&left, &top, &right, &bottom);

		return left;
	}
	else
		return 0;
}

int PrintDevice::GetPaperTopOffset() const
{
	if (printer_controller)
	{
		UINT32 left = 0;
		UINT32 top = 0;
		UINT32 right = 0;
		UINT32 bottom = 0;

		printer_controller->GetOffsets(&left, &top, &right, &bottom);

		return top;
	}
	else 
		return 0;
}

int PrintDevice::GetPaperRightOffset() const
{
	if (printer_controller)
	{
		UINT32 left = 0;
		UINT32 top = 0;
		UINT32 right = 0;
		UINT32 bottom = 0;

		printer_controller->GetOffsets(&left, &top, &right, &bottom);

		return right;
	}
	else 
		return 0;
}

int PrintDevice::GetPaperBottomOffset() const
{
	if (printer_controller)
	{
		UINT32 left = 0;
		UINT32 top = 0;
		UINT32 right = 0;
		UINT32 bottom = 0;

		printer_controller->GetOffsets(&left, &top, &right, &bottom);
		
		return bottom;
	}
	else
		return 0;
}

void PrintDevice::SetScale(UINT32 scale, BOOL updatesize)
{
	if (!printer_controller)
		return;

	VisualDevice::SetScale(scale, updatesize);

	/*LayoutAttr lattr = printPageStyle->GetLayoutAttr();
	lattr.LeftHandOffset = lattr.LeftHandOffset * 100 / scale;
	lattr.RightHandOffset = lattr.RightHandOffset * 100 / scale;
	lattr.LeadingOffset = lattr.LeadingOffset * 100 / scale;
	lattr.TrailingOffset = lattr.TrailingOffset * 100 / scale;
	printPageStyle->SetLayoutAttr(lattr);*/

	printer_controller->SetScale(scale);
}

#endif // _PRINT_SUPPORT_
