/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifdef _PRINT_SUPPORT_

#ifndef _PRINT_DEVICE_H_
#define _PRINT_DEVICE_H_

#include "modules/display/vis_dev.h"
#include "modules/util/simset.h"

class OpPrinterController;

class PrintDevice : public VisualDevice
{
protected:
	OpPrinterController* printer_controller;
		// Constructor for use with PrintPreviewDevice
					PrintDevice(OP_STATUS &result, OpWindow* parentWindow, DocumentManager* doc_man, ScrollType scrolltype) 
								: VisualDevice() 
					{
						scroll_type = scrolltype;
						doc_manager = doc_man;
						result = VisualDevice::Init(parentWindow, doc_man, scrolltype);
					}

public:
					PrintDevice();
					PrintDevice(DocumentManager* doc_man);

	OP_STATUS		Init(OpPrinterController* printerController);
	virtual			~PrintDevice();

	OpPrinterController*
					GetPrinterController() const { return printer_controller; }

	virtual BOOL	NewPage();
	BOOL			StartPage();

	void			GetUserMargins(int& left, int& right, int& top, int& bottom) const;

	virtual int 	GetRenderingViewWidth() const;
	virtual int 	GetRenderingViewHeight() const;
	virtual int 	WinWidth() const { return GetRenderingViewWidth(); }
	virtual int 	WinHeight() const { return GetRenderingViewHeight(); }

	virtual BOOL	IsPrinter() const { return TRUE; }

	virtual void	DrawBgColor(const RECT& rect);
	virtual BOOL	RequireBanding() { return FALSE; }
	virtual BOOL	NextBand(RECT &rect) { return FALSE; }

	void			StartOutput() { logfont.SetHeight(0); } // make sure that font is loaded

	/** Retrieve the horizontal and vertical DPI for the printer */
	virtual void    GetDPI(UINT32* horizontal, UINT32* vertical);

	void			SetScale(UINT32 scale, BOOL updatesize);

	BOOL			IsLandscape();
	virtual int		GetPaperWidth() const;
	virtual int		GetPaperHeight() const;
	virtual int		GetPaperLeftOffset() const;
	virtual int		GetPaperTopOffset() const;
	virtual int		GetPaperRightOffset() const; // offset from right side of page
	virtual int		GetPaperBottomOffset() const; // offset from bottom of page

	virtual void	Update(int x, int y, int width, int height, BOOL update) {}
	virtual void	UpdateAll() {}
};

#endif // _PRINT_DEVICE_H_
#endif // _PRINT_SUPPORT_
