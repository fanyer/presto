/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef NATIVE_PRINTER_INTEGRATION_H
#define NATIVE_PRINTER_INTEGRATION_H

class NativePrinterIntegration
{
public:
	virtual ~NativePrinterIntegration() {}

	/** Initialize printer integration
	  * @return true on success, false on failure
	  */
	virtual bool Init() = 0;

	/** Display printer dialog and let user trigger or cancel printing. */
	virtual bool RunPrintDialog() = 0;

	/** Print. */
	virtual bool Print(const char* print_file_path, const char* print_job_name) = 0;
	virtual bool PageShouldBePrinted(int page_number) = 0;

	virtual bool GetPrinterName(char*& name) = 0;
	virtual bool GetPrinterResolution(int& horizontal, int& vertical) = 0;
	virtual bool GetPrintScale(int& scale) = 0;

	virtual bool GetPaperSize(double& width, double& height) = 0;
	virtual bool GetPaperSizeName(char*& name) = 0;
	virtual bool GetMargins(double& top, double& left, double& bottom, double& right) = 0;	

protected:
};

#endif // NATIVE_PRINTER_INTEGRATION_H
