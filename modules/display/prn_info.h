/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifdef _PRINT_SUPPORT_
#ifndef _PRINTERINFO_H_
#define _PRINTERINFO_H_

#if defined (GENERIC_PRINTING)
# include "modules/display/prndevlisteners.h"
#endif

// Trond added: We decided to go for a preliminary
// print selection solution for the 6.0 release.
// To print the selection, we create a new dummy
// hidden window and paste the selection into it,
// print that dummy window and delete it.
// This code will probably only be used by MSWIN..
// The way it's done IS NOT very nice..
// no time to clean things up.

#if defined MSWIN || defined _UNIX_DESKTOP_
#define PRINT_SELECTION_USING_DUMMY_WINDOW
#endif

class PrintDevice;
class Window;

class PrinterInfo
{
private:

#if defined (GENERIC_PRINTING)
	void *			platform_data;
	PrintListener*	print_listener;
#elif defined (MSWIN)
	HDC				hdc;
	HGLOBAL			pdevmode;
	HGLOBAL			pdevnames;
#endif

	PrintDevice*	print_dev;


#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
	void			MakeSelectionWindow(Window * window, Window ** selection_window);
#endif

public:

	int				next_page;
	int				last_page;
	BOOL			print_selected_only;
	BOOL			user_requested_print_selection; // Set from the print dialog and used by GetPrintDevice

					PrinterInfo();
#if defined(GENERIC_PRINTING)
					PrinterInfo(void * val);
#endif
					~PrinterInfo();

#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
	OP_STATUS       GetPrintDevice(PrintDevice *&pdev, BOOL use_default, BOOL selection, Window* window, Window** selection_window);
#else
	OP_STATUS       GetPrintDevice(PrintDevice *&pdev, BOOL use_default, BOOL selection, Window* window);
#endif

	PrintDevice*	GetPrintDevice() { return print_dev; }
#ifdef MSWIN
	BOOL			GetUserPrinterSettings( HWND hWndParent, BOOL as_settings_dialog, BOOL use_default, BOOL selection);
#endif
};

#ifdef MSWIN
void InitPrinterSettings(HWND hwnd);
void ClearPrinterSettings();  


extern BOOL CALLBACK PrintStopDlg(HWND hDlg, UINT wMessage, WPARAM wParam, LPARAM lParam) ;
extern BOOL CALLBACK PrintAbort(HDC hdcPrinter, int nCode) ;

void CleanUpAfterPrint(Window* window);    
#endif
#endif

LONG ucPrintDocFormatted();
LONG ucPrintPagePrinted(WORD page);
LONG ucPrintingFinished(Window*);
LONG ucPrintingAborted(Window*);

#endif // _PRINT_SUPPORT_
