/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef TOOLKIT_PRINTER_INTEGRATION_H
#define TOOLKIT_PRINTER_INTEGRATION_H

#include "platforms/utilix/x11types.h"

class ToolkitPrinterHelper;

class ToolkitPrinterIntegration
{
public:
	virtual ~ToolkitPrinterIntegration() {}

	/** @return Whether this integration requires a printer helper (ie. can't print
	  * by itself). This affects the Init() call.
	  */
	virtual bool NeedsPrinterHelper() = 0;

	/** @return Whether this integration can print to file without the aid of a 
	  * printer helper  
	  */
	virtual bool SupportsOfflinePrinting() = 0;

	/** Initialize printer integration
	  * @param helper If NeedsPrinterHelper() returns true, this contains a helper
	  *               that is valid for the lifetime of this object. This call takes
	  *               ownership of that pointer.
	  * @return true on success, false on failure
	  */
	virtual bool Init(ToolkitPrinterHelper* helper) = 0;

	// -- all functions below this line will only be called after Init() has returned successfully --

	/**
	 * Set the title that is to be shown in the window decoration
	 * @param caption The caption string
	 */
	virtual void SetCaption(const char* caption) = 0;

	/** Display printer dialog and let user trigger or cancel printing.
	  * @param parent Parent window of printing dialog
	  * @return true if printing should be started, or false if it was canceled
	  */
	virtual bool RunPrintDialog(X11Types::Window parent) = 0;

	/** Print. */
	virtual bool Print(const char* print_file_path, const char* print_job_name) = 0;

	/** @return Whether a certain page should be included in the print
	  */
	virtual bool PageShouldBePrinted(int page_number) = 0;

	/** Get the printer resolution in dpi
	  * @param horizontal Horizontal resolution
	  * @param vertical Vertical resolution
	  * @return true on success, false on failure
	  */
	virtual bool GetPrinterResolution(int& horizontal, int& vertical) = 0;

	/** Get scale to print page at
	  * @param scale Scale to print page at in percent (default: 100)
	  * @return true on success, false on failure
	  */
	virtual bool GetPrintScale(int& scale) = 0;

	/** Get the size of the paper to print on
	  * @param width Width of page in inches
	  * @param height Height of page in inches
	  * @return true on success, false on failure
	  */
	virtual bool GetPaperSize(double& width, double& height) = 0;

	/** Get the margins to use on the page
	  * @param top Top margin in inches
	  * @param left Left margin in inches
	  * @param bottom Bottom margin in inches
	  * @param right Right margin in inches
	  * @return true on success, false on failure
	  */
	virtual bool GetMargins(double& top, double& left, double& bottom, double& right) = 0;

	/** Returns true if user has choosen to print
	  * the selected area only of a document
	  * @return the chosen value
	  */
	virtual bool GetPrintSelectionOnly() = 0;

};

#endif // TOOLKIT_PRINTER_INTEGRATION_H
