/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef VEGAOPPRINTERBACKEND_H
#define VEGAOPPRINTERBACKEND_H

#ifdef VEGA_POSTSCRIPT_PRINTING

/** @short Abstraction of platform printer integration and PostScript printing.
 *
 * Since the VEGAOpPrinterController, different from the OpPrinterControllers,
 * should not be dependent on platform code an abstraction for the platform
 * printing functionality is needed. This includes things like showing a printing
 * dialog, providing information about selected paper size, margins, scaling,
 * and of course the actual print operation (sending the PostScript job to printer).
 * 
 * This class has to be subclassed in the platform code to provide the necessary 
 * print functionality.
 */
class VEGAOpPrinterBackend
{
public:
	/** Create new VEGAOpPrinterBackend object
	  * @param new_printerbackend (output) Will be set to the new
	  * VEGAOpPrinterBackend object, unless an error value is returned.
	  */
	static OP_STATUS Create(VEGAOpPrinterBackend** new_printerbackend);

	virtual ~VEGAOpPrinterBackend() {}

	/** Initiate the printer backend */
	virtual OP_STATUS Init() = 0;

	/** Prepare a printing operation.
	  * Display printer dialog if needed and do other necessary platform preparations.
	  */
	virtual OP_STATUS PreparePrinting() = 0;

	/** Get the printer resolution - DPI
	  * @param horizontal will be set to printer horizontal resolution.
	  * @param vertical will be set to printer vertical resolution.
	  * @return OpStatus::OK if values were successfully set, otherwise OpStatus::ERR.
	  */
	virtual OP_STATUS GetPrinterResolution(UINT16& horizontal, UINT16& vertical) = 0;

	/** Get the print job scaling
	  * @param scale will be set to the scaling of the print job (normal is 100 for 100%). 
	  * @return OpStatus::OK if value was successfully set, otherwise OpStatus::ERR.
	  */
	virtual OP_STATUS GetPrintScale(UINT32& scale) = 0;

	/** Get the print job paper size in inches
	  * Since inches are commonly used in printer metrics (72 dpi, etc) we use them here.
	  * @param width will be set to the paper width for the print job.
	  * @param height will be set to the paper height for the print job.
	  * @return OpStatus::OK if values were successfully set, otherwise OpStatus::ERR.
	  */
	virtual OP_STATUS GetPaperSize(double& width, double& height) = 0;

	/** Get the print job margins in inches
	  * Since inches are commonly used in printer metrics (72 dpi, etc) we use them here.
	  * @param top will be set to the top margin of the print job.
	  * @param left will be set to the left margin of the print job.
	  * @param bottom will be set to the bottom margin of the print job.
	  * @param right will be set to the right margin of the print job.
	  * @return OpStatus::OK if values were successfully set, otherwise OpStatus::ERR.
	  */
	virtual OP_STATUS GetMargins(double& top, double& left, double& bottom, double& right) = 0;

	/** Returns true if only selection should be printed. 
	  * @return the state  
	  */
	virtual bool GetPrintSelectionOnly() = 0;

	/** Print a file with the prepared settings. 
	  * The Print function should at least support PostScript files.
	  * @param print_file file to print.
	  * @param print_job_name a name of the print job. This can be used to identify the print job in platform print interfaces. 
	  * @return OpStatus::OK if the print job was successfully started, otherwise OpStatus::ERR.
	  */
	virtual OP_STATUS Print(class OpFile& print_file, OpString& print_job_name) = 0;
	
	/** Return boolean telling if a numbered page shuould be printed or not.
	  * To support printing of selected pages and ranges of pages this function should
	  * return true if given page number is within the selection of pages that should be printed.
	  * @param page_number page number to check printing for.
	  * @return true if page should be printed, false if page should be omitted.
	  */
	virtual bool PageShouldBePrinted(int page_number) = 0;
};


#endif // VEGA_POSTSCRIPT_PRINTING

#endif // VEGAOPPRINTERBACKEND_H
