/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPPRINTERCONTROLLER_H
#define OPPRINTERCONTROLLER_H

class OpPainter;
class PrinterInfo;

#ifdef GENERIC_PRINTING

class OpWindow;

/** Abstract class to use with OpPrinterController to receive a
 * printevent. Inherit and OnPrint will be called, when it's time to
 * print.
 *
 * FIXME: poor documentation, and also not widely used.
 */
class OpPrintListener
{
public:
	virtual			~OpPrintListener(){};
	virtual void 	OnPrint() = 0;
};
#endif // GENERIC_PRINTING

/** @short Printer controller - paint device for printing
 *
 * FIXME: Revise documentation
 *
 * OpPrinterController is a way to implement "painting on a printer".
 * This is the way that it is planned to be used:
 *
 * The OpPrinterController interface is implemented on a platform.
 *
 * An object of the (platform specific) implementation of
 * OpPrinterController is created, and initialized in whatever way
 * necessary to make the OpPrinterController being able to create an
 * OpPainter that acts on one and only one printer and one and only
 * one papersize, ie the printer has to be completely setup when
 * GetPainter() is called.
 *
 * Under for example Windows this would include eg showing the user a
 * printer selection dialogue, where (s)he could select a specific
 * printer and a specific paper tray.
 *
 * GetPainter() will be the first method to be called from the
 * interface. If non-NULL is returned, the return value is assumed to
 * be an object that has implemented the OpPainter interface
 *
 * If NULL is return this is taken as that printing couldn't be setup
 * properly, and printing won't continue. The object will then be
 * destroyed.
 *
 * Before every page, BeginPage() will be called, to let the
 * implementation handle any setup necessary to start a new page.
 *
 * After every page, EndPage() will be called. Implementations should
 * do what they need here to end the page.
 *
 * This interface should be implemented on platforms that wish to
 * support printing.
 */
class OpPrinterController
{
public:
	/** Create a new OpPrinterController object.
	 *
	 * @param new_printercontroller (output) Will be set to the new
	 * OpPrinterController object, unless an error value is returned.
	 * @param preview Is the new OpPrinterController object going to
	 * be used for print preview or regular printing?
	 */
	static OP_STATUS Create(OpPrinterController** new_printercontroller, BOOL preview=FALSE);

	virtual ~OpPrinterController() {}

#ifdef GENERIC_PRINTING
	/** Initiate the controller. */
	virtual OP_STATUS Init(const OpWindow * opwindow) {return OpStatus::OK; }

	/** A listener that needs to be hit with an OnPrint in PrintDocFormatted and PrintPagePrinted.
	 *
	 * FIXME: documentation
	 */
	virtual void SetPrintListener(OpPrintListener * print_listener) = 0;
#endif // GENERIC_PRINTING

	/** Return an OpPainter for printing.
	 *
	 * The returned OpPainter can draw its primitives to a printer
	 * that uses this OpPrinterController as its paint device.
	 *
	 * @return an OpPainter that paints on a printer.
	 */
	virtual const OpPainter* GetPainter() = 0;

	/** Prepare for printing to a new page. */
	virtual OP_STATUS BeginPage() = 0;

	/** End the current page. */
	virtual OP_STATUS EndPage() = 0;

#ifdef GENERIC_PRINTING
	/**	Initiate printing of a document */
	virtual void PrintDocFormatted(PrinterInfo * printer_info) = 0;

	/** Is called prior to printing of subsequent pages (PrintDocFormatted handles first page) */
	virtual void PrintPagePrinted(PrinterInfo * printer_info) = 0;

	/**	Is called when finished printing */
	virtual void PrintDocFinished() = 0;

	/** Is called when aborting printing */
	virtual void PrintDocAborted() = 0;
#endif // GENERIC_PRINTING

	/** Get the size of the paper in pixels. */
	virtual void GetSize(UINT32* w, UINT32* h) = 0;

	/** Get the offsets (the printer's built-in margins).
	 *
	 * The left offset is counted from the left side and the right
	 * offset is counted from the right side, and so on.
	 */
	virtual void GetOffsets(UINT32* left, UINT32* top, UINT32* right, UINT32* bottom) = 0;

	/** Get DPI (dots per inch). */
	virtual void GetDPI(UINT32* x, UINT32* y) = 0;

   	/** Scale the printer's coordinate system, in percent. */
	virtual void SetScale(UINT32 scale) = 0;

	/** Get the current scale factor. Initially it is 100%. */
	virtual UINT32 GetScale() = 0;

	/** Return true if the printer is monochrome. */
	virtual BOOL IsMonochrome() = 0;

#ifdef PI_PRINT_SELECTION
	/** Return true if only selection should be printed. */
	virtual BOOL GetPrintSelectionOnly() = 0;
#endif
};

#endif // OPPRINTERCONTROLLER_H
