/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef KDE4_PRINTER_INTEGRATION_H
#define KDE4_PRINTER_INTEGRATION_H

#include "platforms/quix/toolkits/ToolkitPrinterIntegration.h"

class QPrinter;
#include <QString>

class Kde4PrinterIntegration : public ToolkitPrinterIntegration
{
public:
	Kde4PrinterIntegration();
	virtual ~Kde4PrinterIntegration();

	// From ToolkitPrinterIntegration
	virtual bool NeedsPrinterHelper() { return true; }
	virtual bool SupportsOfflinePrinting() { return false; }
	virtual bool Init(ToolkitPrinterHelper* helper);
	virtual void SetCaption(const char* caption);

	virtual bool RunPrintDialog(X11Types::Window parent);

	virtual bool Print(const char* print_file_path, const char* print_job_name);
	virtual bool PageShouldBePrinted(int page_number);

	virtual bool GetPrinterResolution(int& horizontal, int& vertical);
	virtual bool GetPrintScale(int& scale);

	virtual bool GetPaperSize(double& width, double& height);
	virtual bool GetMargins(double& top, double& left, double& bottom, double& right);

	virtual bool GetPrintSelectionOnly() { return false; }

private:
	QPrinter* m_printer;
	ToolkitPrinterHelper* m_helper;
	QString m_caption;
};

#endif // KDE4_PRINTER_INTEGRATION_H
