/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#include "core/pch.h"

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/libvega/src/postscript/vegaopprintercontroller.h"
#include "modules/libvega/src/postscript/vegaopprinterbackend.h"
#include "modules/libvega/src/postscript/postscriptvegaprinterlistener.h"

#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/pi/OpWindow.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/util/filefun.h"


/** Create platform VEGAOpPrinterController */
OP_STATUS OpPrinterController::Create(OpPrinterController** new_printercontroller, BOOL preview/*=FALSE*/)
{
	OP_ASSERT(new_printercontroller != NULL);

	// Create platform specific printer backend that will be used to get print job metrics
	// and to send the finished result to printer
	VEGAOpPrinterBackend* new_vega_printer_backend;
	RETURN_IF_ERROR(VEGAOpPrinterBackend::Create(&new_vega_printer_backend));
	OpAutoPtr<VEGAOpPrinterBackend> new_vega_printer_backend_pointer(new_vega_printer_backend);
	RETURN_IF_ERROR(new_vega_printer_backend->Init());

	if (!preview)
	{
		// Prepare platform printing. This might cause blocking printing dialogs to appear.
		RETURN_IF_ERROR(new_vega_printer_backend->PreparePrinting());
	}

	*new_printercontroller = OP_NEW(VEGAOpPrinterController, (new_vega_printer_backend, preview));
	if (*new_printercontroller == NULL)
		return OpStatus::ERR_NO_MEMORY;
	new_vega_printer_backend_pointer.release();
	return OpStatus::OK;
}


VEGAOpPrinterController::VEGAOpPrinterController(VEGAOpPrinterBackend* vega_printer_backend, BOOL preview)
	: vega_printer_backend(vega_printer_backend),
	  scale(0),
	  preview(preview),
	  painter(NULL),
	  vega_printer_listener(NULL)
{
}


VEGAOpPrinterController::~VEGAOpPrinterController()
{
	if (painter)
		OP_DELETE(painter);

	if (vega_printer_listener)
		OP_DELETE(vega_printer_listener);

	if (vega_printer_backend)
		OP_DELETE(vega_printer_backend);
}


#ifdef GENERIC_PRINTING
OP_STATUS VEGAOpPrinterController::Init(const OpWindow * opwindow)
{
	title.Set(opwindow->GetTitle());

	// Fallback default values
	SetScale(100);

	paper_metrics.width = 595;		// A4 width in points
	paper_metrics.height = 842;		// A4 height in points

	paper_metrics.offset_top = 30;
	paper_metrics.offset_left = 40;
	paper_metrics.offset_bottom = 40;
	paper_metrics.offset_right = 30;

	paper_metrics.dpi_x = 72;		// PostScript standard
	paper_metrics.dpi_y = 72;		// PostScript standard

	UINT32 horizontal_dpi;
	UINT32 vertical_dpi;
	if (OpStatus::IsSuccess(g_op_screen_info->GetDPI(&horizontal_dpi, &vertical_dpi, const_cast<OpWindow*>(opwindow))))
	{
		screen_metrics.dpi_x = horizontal_dpi;
		screen_metrics.dpi_y = vertical_dpi;
	}

	RETURN_IF_ERROR(vega_printer_backend->GetPrintScale(scale));

#define ROUND(v) ((UINT32)(0.5 + (v)))

	double w, h;
	if (OpStatus::IsSuccess(vega_printer_backend->GetPaperSize(w, h)))
	{
		paper_metrics.width = ROUND(w * paper_metrics.dpi_x);
		paper_metrics.height = ROUND(h * paper_metrics.dpi_y);
	}

	double top, left, bottom, right;
	if (OpStatus::IsSuccess(vega_printer_backend->GetMargins(top, left, bottom, right)))
	{
		paper_metrics.offset_top = ROUND(top * paper_metrics.dpi_y);
		paper_metrics.offset_left = ROUND(left * paper_metrics.dpi_x);
		paper_metrics.offset_bottom = ROUND(bottom * paper_metrics.dpi_y);
		paper_metrics.offset_right = ROUND(right * paper_metrics.dpi_x);
	}

#undef ROUND

	// Get the sizes of the actual print area (paper size - margins)
	GetSize(&screen_metrics.width, &screen_metrics.height);

	if (!preview)
	{
		OpString printfilename;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TEMP_FOLDER, printfilename));
		RETURN_IF_ERROR(printfilename.Append(UNI_L("operaprint.ps")));
#ifdef CREATE_UNIQUE_FILENAME
		RETURN_IF_ERROR(CreateUniqueFilename(printfilename));
#endif // CREATE_UNIQUE_FILENAME
		RETURN_IF_ERROR(ps_file.Construct(printfilename));

		// The PostScript generation is done by a PostScriptVEGAPrinterListener that
		// is registered as listener to the VEGAOpPainter.
		vega_printer_listener = OP_NEW(PostScriptVEGAPrinterListener, (ps_file));
		if (!vega_printer_listener)
			return OpStatus::ERR_NO_MEMORY;
		vega_printer_listener->Init(&screen_metrics, &paper_metrics);

		// This VEGAOpPainter will be used to paint the print details.
		painter = OP_NEW(VEGAOpPainter, ());
		if (!painter)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(painter->Construct(screen_metrics.width, screen_metrics.height));

		((VEGAOpPainter*)painter)->setPrinterListener(vega_printer_listener);
	}

	return OpStatus::OK;
}

void VEGAOpPrinterController::PrintDocFormatted(PrinterInfo *printer_info)
{
	if(!print_listener)
		return;
	print_listener->OnPrint();
}


void VEGAOpPrinterController::PrintPagePrinted(PrinterInfo *printer_info)
{
	if(!print_listener)
		return;
	print_listener->OnPrint();
}


void VEGAOpPrinterController::PrintDocFinished()
{
	if (vega_printer_listener)
		vega_printer_listener->printFinished();

	if (!preview && vega_printer_backend)
		vega_printer_backend->Print(ps_file, title);
}


void VEGAOpPrinterController::PrintDocAborted()
{
	if (vega_printer_listener)
		vega_printer_listener->printAborted();
}

#endif // GENERIC_PRINTING


const OpPainter *VEGAOpPrinterController::GetPainter()
{
	return (const OpPainter*) painter;
}


OP_STATUS VEGAOpPrinterController::BeginPage()
{
	if (vega_printer_listener)
		return vega_printer_listener->startPage();

	return OpStatus::OK;
}


OP_STATUS VEGAOpPrinterController::EndPage()
{
	if (vega_printer_listener)
		RETURN_IF_ERROR(vega_printer_listener->endPage());

	return OpStatus::OK;
}


void VEGAOpPrinterController::GetSize(UINT32 *width, UINT32 *height)
{
	if (scale <= 0)
		SetScale(100);

	*width  = paper_metrics.dpi_x ? (paper_metrics.width - paper_metrics.offset_left - paper_metrics.offset_right) *
				screen_metrics.dpi_x / paper_metrics.dpi_x * 100 / scale : 0;

	*height = paper_metrics.dpi_y ? (paper_metrics.height - paper_metrics.offset_top - paper_metrics.offset_bottom) *
				screen_metrics.dpi_y / paper_metrics.dpi_y * 100 / scale : 0;
}


void VEGAOpPrinterController::GetOffsets(UINT32 *left, UINT32 *top, UINT32 *right, UINT32 *bottom)
{
	if (scale <= 0)
		SetScale(100);

	// Margins are handled by PostScript translate and scaling. The screen_metrics offsets will all be 0.
	if (preview)
	{
		*left	= paper_metrics.dpi_x ? screen_metrics.offset_left	 * screen_metrics.dpi_x / paper_metrics.dpi_x * 100 / scale : 0;
		*top	= paper_metrics.dpi_y ? screen_metrics.offset_top	 * screen_metrics.dpi_y / paper_metrics.dpi_y * 100 / scale : 0;
		*right	= paper_metrics.dpi_x ? screen_metrics.offset_right	 * screen_metrics.dpi_x / paper_metrics.dpi_x * 100 / scale : 0;
		*bottom	= paper_metrics.dpi_y ? screen_metrics.offset_bottom * screen_metrics.dpi_y / paper_metrics.dpi_y * 100 / scale : 0;
	}
	else
	{
		*top = screen_metrics.offset_top;
		*left = screen_metrics.offset_left;
		*bottom = screen_metrics.offset_bottom;
		*right = screen_metrics.offset_right;
	}
}


void VEGAOpPrinterController::GetDPI(UINT32* x, UINT32* y)
{
	if (x)
		*x = screen_metrics.dpi_x;
	if (y)
		*y = screen_metrics.dpi_y;
}


UINT32 VEGAOpPrinterController::GetScale()
{
	return scale;
}


void VEGAOpPrinterController::SetScale(UINT32 scale)
{
	this->scale = scale;
}


BOOL VEGAOpPrinterController::IsMonochrome()
{
	return FALSE;
}

#ifdef PI_PRINT_SELECTION
BOOL VEGAOpPrinterController::GetPrintSelectionOnly()
{
	return vega_printer_backend && vega_printer_backend->GetPrintSelectionOnly();
}
#endif

#endif // VEGA_POSTSCRIPT_PRINTING
