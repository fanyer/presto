/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/MacOpPrinterController.h"
#include "platforms/mac/pi/MacOpScreenInfo.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/OpDelayedDelete.h"
#include "platforms/mac/util/MachOCompatibility.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/dochand/win.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "platforms/mac/pi/MacVegaPrinterListener.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"

#define BOOL NSBOOL
#import <Foundation/NSString.h>
#import <AppKit/NSPrintInfo.h>
#import <AppKit/NSPrintOperation.h>
#import <AppKit/NSPrintPanel.h>
#import <AppKit/NSView.h>
#import <AppKit/NSButton.h>
#undef BOOL

#define kPrinterScaleFactor (1.4)

static NSString *g_backgoundCaption = NULL;
static NSString *g_headersCaption = NULL;
static NSString *g_msrCaption = NULL;

/*static*/
PMPageFormat MacOpPrinterController::s_pageFormat = NULL;

/*static*/
PMPrintSession MacOpPrinterController::s_printSession = NULL;

/*static*/
PMPrintSettings MacOpPrinterController::s_printSettings = NULL;

/*static*/
int MacOpPrinterController::s_refCount = 0;

/*static*/
bool MacOpPrinterController::m_printing = false;

OP_STATUS OpPrinterController::Create(OpPrinterController** new_printercontroller, BOOL preview/*=FALSE*/)
{
	OP_ASSERT(new_printercontroller != NULL);
	*new_printercontroller = new MacOpPrinterController();
	if (*new_printercontroller == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

MacOpPrinterController::MacOpPrinterController()
{
	m_painter = NULL;
	m_scale = 100;
	m_pageFormatScale = 100;
	m_paperWidth = 800;		//if size <= 0, core may freeze up in an infinite loop
	m_paperHeight = 1000;
	m_printing = false;
	m_pageOpen = false;
	m_bitmap = NULL;

	InitStatic();
}

MacOpPrinterController::~MacOpPrinterController()
{
	EndSession();

	delete m_painter;

	FreeStatic();
}

OP_STATUS MacOpPrinterController::Init(const OpWindow * opwindow)
{
	m_painter = NULL;
	m_printer_listener = NULL;

	return OpStatus::OK;
}

void MacOpPrinterController::PrintDocFormatted(PrinterInfo *printer_info)
{
	if (!m_printListener)
		return;

	m_printListener->OnPrint();
}

void MacOpPrinterController::PrintPagePrinted(PrinterInfo *printer_info)
{
	if (!m_printListener)
		return;

	m_printListener->OnPrint();
}

void MacOpPrinterController::PrintDocFinished()
{
}

void MacOpPrinterController::PrintDocAborted()
{
}

const OpPainter *MacOpPrinterController::GetPainter()
{
	if (!m_printing)
	{
		StartSession();
	}
	return (const OpPainter*) m_painter;
}

OP_STATUS MacOpPrinterController::BeginPage()
{
	OSStatus err;

	err = PMSessionError(s_printSession);
	if (err == kPMCancel)
	{
		return OpStatus::ERR;
	}
	if (!m_printing)
	{
		StartSession();
	}

    err = PMSessionBeginPageNoDialog(s_printSession, s_pageFormat, NULL);
    if (!err)
    {
        CGContextRef printingContext = NULL;
        err = PMSessionGetCGGraphicsContext(s_printSession, &printingContext);
        m_pageOpen = true;
        
        if (!err)
        {
            PMPaper paper;
            PMGetPageFormatPaper(s_pageFormat, &paper);
            PMPaperMargins paperMargins;
            PMPaperGetMargins(paper, &paperMargins);
            CGContextTranslateCTM(printingContext, paperMargins.left * kPrinterScaleFactor, paperMargins.bottom * kPrinterScaleFactor);
            m_printer_listener->SetGraphicsContext(printingContext);
            m_printer_listener->SetWindowHeight(m_paperHeight);
            
            return OpStatus::OK;
        }
    }
    
    PMSessionEndPageNoDialog(s_printSession);
	PMSessionError(s_printSession);

	return OpStatus::ERR;
}

OP_STATUS MacOpPrinterController::EndPage()
{
	if (m_pageOpen)
	{
        m_printer_listener->SetGraphicsContext((CGContextRef)NULL);		
        PMSessionEndPageNoDialog(s_printSession);
		m_pageOpen = false;
	}

	return OpStatus::OK;
}

void MacOpPrinterController::StartSession()
{
	PMRetain(s_printSession);
	PMRetain(s_printSettings);
	PMRetain(s_pageFormat);

	double scale;
	PMGetScale(s_pageFormat, &m_pageFormatScale);
	scale = m_pageFormatScale;
	scale /= kPrinterScaleFactor;
	PMSetScale(s_pageFormat, scale);
	
	Boolean result;
	PMSessionValidatePageFormat(s_printSession, s_pageFormat, &result);
	
    PMSessionBeginCGDocumentNoDialog(s_printSession, s_printSettings, s_pageFormat);
	
	m_printing = true;
	
	PMRect paperRect;
	PMGetAdjustedPageRect(s_pageFormat, &paperRect);

	// Make sure Opera's printer scale is always 100%
	// Opera will multiply its print scale with that of the printer setting.
	TRAPD(rc, g_pcprint->WriteIntegerL(PrefsCollectionPrint::PrinterScale, 100));

	m_paperWidth = (UINT32)(paperRect.right - paperRect.left);
	m_paperHeight = (UINT32)(paperRect.bottom - paperRect.top);

	if (!m_printer_listener)
		m_printer_listener = new MacVegaPrinterListener;
	if (!m_painter)
	{
		if (OpStatus::IsSuccess(OpBitmap::Create(&m_bitmap, m_paperWidth, m_paperHeight, FALSE, TRUE, 0, 0, TRUE)))
			m_painter = m_bitmap->GetPainter();
		if (m_painter)
			((VEGAOpPainter*)m_painter)->setPrinterListener(m_printer_listener);
	}
}

void MacOpPrinterController::EndSession()
{
	if (m_printing)
	{
		EndPage();
		PMSetScale(s_pageFormat, m_pageFormatScale);
		Boolean result;
		PMSessionValidatePageFormat(s_printSession, s_pageFormat, &result);
        PMSessionEndDocumentNoDialog(s_printSession);
		m_printing = false;
		new OpDelayedDelete<OpBitmap>(m_bitmap);
		m_bitmap = NULL;
		m_painter = NULL;
	}
}

/*static*/
void MacOpPrinterController::InitStatic(void)
{
	if (s_refCount == 0)
	{
		if (PMCreatePageFormat(&s_pageFormat) != noErr)
		{
			return;
		}

		if (PMCreatePrintSettings(&s_printSettings) != noErr)
		{
			return;
		}

        OpString string;
        TRAPD(status, g_languageManager->GetStringL(Str::DI_IDM_PRINT_BACKGROUND_TOGGLE, string));
        if (OpStatus::IsSuccess(status))
        {
            g_backgoundCaption = [NSString stringWithCharacters:(const unichar*)string.CStr() length:string.Length()];
            [g_backgoundCaption retain];
        }
        TRAP(status, g_languageManager->GetStringL(Str::DI_IDM_PRINT_HEADER_TOGGLE, string));
        if (OpStatus::IsSuccess(status))
        {
            g_headersCaption = [NSString stringWithCharacters:(const unichar*)string.CStr() length:string.Length()];
            [g_headersCaption retain];
        }
        TRAP(status, g_languageManager->GetStringL(Str::D_PRINT_DIALOG_FIT_TO_WIDTH, string));
        if (OpStatus::IsSuccess(status))
        {
            g_msrCaption = [NSString stringWithCharacters:(const unichar*)string.CStr() length:string.Length()];
            [g_msrCaption retain];
        }
	}

	s_refCount++;
}

/*static*/
void MacOpPrinterController::PrintWindow(Window *window)
{
	Boolean accepted;
	accepted = GetPrintDialog(NULL, window->GetWindowTitle());
	if (accepted)
	{
		window->GetMessageHandler()->PostMessage(DOC_START_PRINTING, 0, 0);
	}
}

/*static*/
Boolean MacOpPrinterController::GetPrintDialog(WindowRef sheetParentWindow /* = NULL*/, const uni_char* jobTitle /* = NULL */)
{
	Boolean accepted;

    NSRect frame = NSMakeRect(0, 0, 200, 60);
    NSView *accView = [[NSView alloc] initWithFrame:frame];
    
    frame = NSMakeRect(0, 0, 200, 20);
    NSButton *msrButton = [[NSButton alloc] initWithFrame:frame];
#ifdef MSR_PRINTING
    [msrButton setState:g_pcprint->GetIntegerPref(PrefsCollectionPrint::FitToWidthPrint)];
#endif
    if (g_msrCaption)
        [msrButton setTitle:g_msrCaption];
    else
        [msrButton setTitle:@"Fit to Width"];
    [msrButton setButtonType:NSSwitchButton];
    [accView addSubview:msrButton];
    
    frame = NSMakeRect(0, 20, 200, 20);
    NSButton *headersButton = [[NSButton alloc] initWithFrame:frame];
    [headersButton setState:g_pcprint->GetIntegerPref(PrefsCollectionPrint::ShowPrintHeader)];
    if (g_headersCaption)
        [headersButton setTitle:g_headersCaption];
    else
        [headersButton setTitle:@"Print Headers and Footers"];
    [headersButton setButtonType:NSSwitchButton];
    [accView addSubview:headersButton];
    
    frame = NSMakeRect(0, 40, 200, 20);
    NSButton *backgroundButton = [[NSButton alloc] initWithFrame:frame];
    [backgroundButton setState:g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrintBackground)];
    if (g_backgoundCaption)
        [backgroundButton setTitle:g_backgoundCaption];
    else
        [backgroundButton setTitle:@"Print Background"];
    [backgroundButton setButtonType:NSSwitchButton];
    [accView addSubview:backgroundButton];
    
    NSPrintOperation *printOp = [NSPrintOperation printOperationWithView:nil];
    NSPrintPanel *printPanel = [printOp printPanel];
    [printPanel setOptions:0x103];	// NSPrintPanelShowsCopies + NSPrintPanelShowsPageRange + NSPrintPanelShowsPageSetupAccessory
    [printOp setAccessoryView:accView];
    if (jobTitle)
        [printOp setJobTitle:[NSString stringWithCharacters:(const unichar*)jobTitle length:uni_strlen(jobTitle)]];
    [printOp runOperation];
    NSPrintInfo *printInfo = [printOp printInfo];
    NSString *jobDisposition = [printInfo jobDisposition];
    accepted = (jobDisposition != NSPrintCancelJob);
    s_printSession = (PMPrintSession)[printInfo PMPrintSession];
    PMCopyPrintSettings((PMPrintSettings)[printInfo PMPrintSettings], s_printSettings);
    PMCopyPageFormat((PMPageFormat)[printInfo PMPageFormat], s_pageFormat);
    
    if (accepted)
    {
        BOOL background = [backgroundButton state];
        BOOL headers = [headersButton state];
        BOOL msrprint = [msrButton state];
        
        TRAPD(rc, g_pcprint->WriteIntegerL(PrefsCollectionPrint::PrintBackground, background));
        TRAP(rc, g_pcprint->WriteIntegerL(PrefsCollectionPrint::ShowPrintHeader, headers));
#ifdef MSR_PRINTING
        TRAP(rc, g_pcprint->WriteIntegerL(PrefsCollectionPrint::FitToWidthPrint, msrprint));
#endif
    }
    
    [accView release];
    [msrButton release];
    [headersButton release];
    [backgroundButton release];

	return accepted;
}

/*static*/
void MacOpPrinterController::FreeStatic(void)
{
	s_refCount--;
	if (s_refCount == 0)
	{
		PMRelease(s_pageFormat);
		s_pageFormat = 0;

		PMRelease(s_printSettings);
		s_printSettings = 0;

		PMRelease(s_printSession);
		s_printSession = 0;

        [g_backgoundCaption release];
        [g_headersCaption release];
        [g_msrCaption release];
	}
}

void MacOpPrinterController::GetSize(UINT32 *width, UINT32 *height)
{
	*width = m_paperWidth;
	*height = m_paperHeight;
}

void MacOpPrinterController::GetOffsets(UINT32 *left, UINT32 *top, UINT32 *right, UINT32 *bottom)
{
	*left = 0;
	*top = 0;
	*right = 0;
	*bottom = 0;
}

void MacOpPrinterController::GetDPI(UINT32* x, UINT32* y)
{
	if (x)
		*x = 72;
	if (y)
		*y = 72;
}

void MacOpPrinterController::SetScale(UINT32 scale)
{
	m_scale = scale;
}

BOOL MacOpPrinterController::IsMonochrome()
{
    return FALSE;
}

/*static*/
int MacOpPrinterController::GetSelectedStartPage()
{
	UInt32 page = 1;
	if (s_printSettings)
		::PMGetFirstPage(s_printSettings, &page);
	return page;
}

/*static*/
int MacOpPrinterController::GetSelectedEndPage()
{
	UInt32 page = USHRT_MAX;
	if (s_printSettings)
		::PMGetLastPage(s_printSettings, &page);
	return page;
}

