/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef VEGAOPPRINTERCONTROLLER_H
#define VEGAOPPRINTERCONTROLLER_H

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/pi/OpPrinterController.h"
#include "modules/util/opfile/opfile.h"

// Needed for PostScriptOutputMetrics definition
#include "modules/libvega/src/postscript/postscriptcommandbuffer.h"


class VEGAOpPainter;
class OpWindow;
class VEGAOpPrinterBackend;


class VEGAOpPrinterController : public OpPrinterController
{
public:
	VEGAOpPrinterController(VEGAOpPrinterBackend* vega_printer_backend, BOOL preview);
	~VEGAOpPrinterController();

#ifdef GENERIC_PRINTING
	virtual OP_STATUS Init(const OpWindow * opwindow);
	virtual void SetPrintListener(OpPrintListener *print_listener) { this->print_listener = print_listener; }
#endif

	virtual const OpPainter *GetPainter();

	virtual OP_STATUS BeginPage();
	virtual OP_STATUS EndPage();

#ifdef GENERIC_PRINTING
	virtual void PrintDocFormatted(PrinterInfo *printer_info);
	virtual void PrintPagePrinted(PrinterInfo *printer_info);
	virtual void PrintDocFinished();
	virtual void PrintDocAborted();
#endif

	virtual void GetSize(UINT32 *w, UINT32 *h);
	virtual void GetOffsets(UINT32 *w, UINT32 *h, UINT32 *right, UINT32 *bottom);
	virtual void GetDPI(UINT32* x, UINT32* y);
	virtual UINT32 GetScale();
	virtual void SetScale(UINT32 scale);

	virtual BOOL IsMonochrome();

#ifdef PI_PRINT_SELECTION
	virtual BOOL GetPrintSelectionOnly();
#endif

private:
	VEGAOpPrinterBackend* vega_printer_backend;
	UINT32 scale;

	PostScriptOutputMetrics screen_metrics;
	PostScriptOutputMetrics paper_metrics;

	OpFile ps_file;
	OpString title;

#ifdef GENERIC_PRINTING
	OpPrintListener *print_listener;
#endif

	BOOL preview;
	VEGAOpPainter* painter;
	class PostScriptVEGAPrinterListener* vega_printer_listener;
};


#endif // VEGA_POSTSCRIPT_PRINTING

#endif // VEGAOPPRINTERCONTROLLER_H
